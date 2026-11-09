/**
 * @file SshSubsystemSftp.cpp
 * @brief SFTP subsystem handler implementation using libssh's SFTP server API.
 */

#include <dirent.h>
#include <sys/stat.h>

#include <sihd/sys/platform.hpp>

#include "sihd/ssh/utils.hpp"

#if !defined(__SIHD_WINDOWS__)
# include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>

// Define WITH_SERVER before including sftp.h to get sftp_server_* functions
#define WITH_SERVER 1

#include <map>

#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/sftp.h>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshSubsystemSftp.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/endian.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#if defined(__SIHD_WINDOWS__)
# ifndef PATH_MAX
#  define PATH_MAX MAX_PATH
# endif
# ifndef S_ISLNK
#  define S_ISLNK(m) (0)
# endif
#endif

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::ssh
{

using namespace sihd::sys;

SIHD_LOGGER;

namespace
{

constexpr size_t packet_length_size = sizeof(uint32_t);

// sftp packets are a big endian length then that many bytes
size_t complete_packets(const uint8_t *data, size_t len)
{
    size_t count = 0;
    size_t offset = 0;
    while (data != nullptr && len - offset >= packet_length_size)
    {
        const uint32_t size = endian::read_be<uint32_t>(data + offset, packet_length_size);
        if (len - offset - packet_length_size < size)
            break;
        offset += packet_length_size + size;
        ++count;
    }
    return count;
}

// a READ asks for any length: answer at most what OpenSSH's sftp-server does, short reads are legal
constexpr uint32_t max_read_length = 256 * 1024 - 1024;
// a whole directory in one NAME packet outgrows the 256 KiB an OpenSSH client accepts
constexpr size_t max_readdir_entries = 100;

constexpr std::array<std::pair<mode_t, char>, 9> permission_bits {{
    {S_IRUSR, 'r'},
    {S_IWUSR, 'w'},
    {S_IXUSR, 'x'},
    {S_IRGRP, 'r'},
    {S_IWGRP, 'w'},
    {S_IXGRP, 'x'},
    {S_IROTH, 'r'},
    {S_IWOTH, 'w'},
    {S_IXOTH, 'x'},
}};

#if defined(__SIHD_WINDOWS__)

// struct stat has a 32-bit st_size on windows
using FileStat = struct _stat64;

int stat_file(const std::string & path, FileStat & st, bool)
{
    return ::_stat64(path.c_str(), &st);
}

#else

using FileStat = struct stat;

int stat_file(const std::string & path, FileStat & st, bool follow_links)
{
    return follow_links ? ::stat(path.c_str(), &st) : ::lstat(path.c_str(), &st);
}

#endif

SshSubsystemSftp::SftpAttributes attributes_from_stat(const std::string & name, const FileStat & st)
{
    SshSubsystemSftp::SftpAttributes attrs;
    attrs.name = name;
    // Keep full mode including file type bits (S_IFDIR, S_IFREG, etc.)
    // SFTP v3 derives file type from the mode field, not a separate type field
    attrs.permissions = st.st_mode;
    attrs.uid = st.st_uid;
    attrs.gid = st.st_gid;
    attrs.size = st.st_size;
    attrs.atime = st.st_atime;
    attrs.mtime = st.st_mtime;

    if (S_ISDIR(st.st_mode))
        attrs.type = SSH_FILEXFER_TYPE_DIRECTORY;
    else if (S_ISLNK(st.st_mode))
        attrs.type = SSH_FILEXFER_TYPE_SYMLINK;
    else if (S_ISREG(st.st_mode))
        attrs.type = SSH_FILEXFER_TYPE_REGULAR;
    else
        attrs.type = SSH_FILEXFER_TYPE_SPECIAL;

    std::string mode_str(1, S_ISDIR(st.st_mode) ? 'd' : (S_ISLNK(st.st_mode) ? 'l' : '-'));
    for (const auto & [bit, letter] : permission_bits)
        mode_str += (st.st_mode & bit) ? letter : '-';

    std::array<char, 64> time_str;
    struct tm tm_info = time::to_tm(st.st_mtime * static_cast<int64_t>(1e9), true);
    strftime(time_str.data(), time_str.size(), "%b %d %H:%M", &tm_info);

    attrs.longname = fmt::format("{} {:3d} {:5d} {:5d} {:8d} {} {}",
                                 mode_str,
                                 static_cast<int>(st.st_nlink),
                                 st.st_uid,
                                 st.st_gid,
                                 attrs.size,
                                 time_str.data(),
                                 name);

    return attrs;
}

} // namespace

struct SshSubsystemSftp::Impl
{
        SshChannel *channel = nullptr;
        sftp_session_struct *sftp = nullptr;
        bool running = false;
        bool initialized = false;
        bool closed = false;
        std::string root_path;
        uint64_t next_handle_id = 1;
        std::map<std::string, std::string> handle_to_path;
        std::map<std::string, sihd::sys::File> handle_to_file;
        std::map<std::string, size_t> dir_positions;
        // sftp_get_client_message spins until a whole packet arrived: only complete ones are read
        size_t ready_packets = 0;

        bool do_init();
        void handle_message(sftp_client_message_struct *msg);
        void handle_realpath(sftp_client_message_struct *msg);
        void handle_stat(sftp_client_message_struct *msg, bool follow_links);
        void handle_opendir(sftp_client_message_struct *msg);
        void handle_readdir(sftp_client_message_struct *msg);
        void handle_close(sftp_client_message_struct *msg);

        void handle_mkdir(sftp_client_message_struct *msg);
        void handle_rmdir(sftp_client_message_struct *msg);
        void handle_open(sftp_client_message_struct *msg);
        void handle_read(sftp_client_message_struct *msg);
        void handle_write(sftp_client_message_struct *msg);

        void handle_remove(sftp_client_message_struct *msg);
        void handle_rename(sftp_client_message_struct *msg);
        void handle_setstat(sftp_client_message_struct *msg);
        void handle_readlink(sftp_client_message_struct *msg);
        void handle_symlink(sftp_client_message_struct *msg);

        Result<std::string> resolve_path(const std::string & path);
        Result<std::string> resolve_entry_path(const std::string & path);
        bool reply_if_refused(sftp_client_message_struct *msg, const Result<std::string> & resolved);
        std::string generate_handle();
        void reply_status(sftp_client_message_struct *msg, uint32_t status, const std::string & message = "");
};

SshSubsystemSftp::StatResult SshSubsystemSftp::StatResult::ok(const SftpAttributes & attrs)
{
    return {true, SSH_FX_OK, attrs};
}

SshSubsystemSftp::StatResult SshSubsystemSftp::StatResult::error(uint32_t code)
{
    return {false, code, {}};
}

SshSubsystemSftp::ReaddirResult SshSubsystemSftp::ReaddirResult::ok(std::vector<SftpAttributes> entries, bool eof)
{
    return {true, eof, SSH_FX_OK, std::move(entries)};
}

SshSubsystemSftp::ReaddirResult SshSubsystemSftp::ReaddirResult::end()
{
    return {true, true, SSH_FX_EOF, {}};
}

SshSubsystemSftp::ReaddirResult SshSubsystemSftp::ReaddirResult::error(uint32_t code)
{
    return {false, false, code, {}};
}

SshSubsystemSftp::ReadResult SshSubsystemSftp::ReadResult::ok(std::vector<char> data)
{
    return {true, false, SSH_FX_OK, std::move(data)};
}

SshSubsystemSftp::ReadResult SshSubsystemSftp::ReadResult::end()
{
    return {true, true, SSH_FX_EOF, {}};
}

SshSubsystemSftp::ReadResult SshSubsystemSftp::ReadResult::error(uint32_t code)
{
    return {false, false, code, {}};
}

SshSubsystemSftp::OpResult SshSubsystemSftp::OpResult::ok()
{
    return {true, SSH_FX_OK, ""};
}

SshSubsystemSftp::OpResult SshSubsystemSftp::OpResult::error(uint32_t code, const std::string & msg)
{
    return {false, code, msg};
}

namespace
{

sftp_attributes create_sftp_attributes(const SshSubsystemSftp::SftpAttributes & attrs)
{
    sftp_attributes sa = static_cast<sftp_attributes>(calloc(1, sizeof(struct sftp_attributes_struct)));
    if (!sa)
        return nullptr;

    sa->name = strdup(attrs.name.c_str());
    sa->longname = strdup(attrs.longname.c_str());
    sa->permissions = static_cast<uint32_t>(attrs.permissions);
    sa->uid = static_cast<uint32_t>(attrs.uid);
    sa->gid = static_cast<uint32_t>(attrs.gid);
    sa->size = attrs.size;
    sa->atime = attrs.atime;
    sa->mtime = attrs.mtime;
    sa->type = attrs.type;
    sa->flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_UIDGID
                | SSH_FILEXFER_ATTR_ACMODTIME;

    return sa;
}

uint32_t sftp_status_from_error(const Error & err)
{
    switch (err.code)
    {
        case invalid_argument:
        case overflow:
            return SSH_FX_BAD_MESSAGE;
        case not_found:
            return SSH_FX_NO_SUCH_FILE;
        case permission_denied:
            return SSH_FX_PERMISSION_DENIED;
        case not_supported:
            return SSH_FX_OP_UNSUPPORTED;
        case already_exists:
            return SSH_FX_FILE_ALREADY_EXISTS;
        case closed:
            return SSH_FX_NO_CONNECTION;
        default:
            return SSH_FX_FAILURE;
    }
}

} // namespace

SshSubsystemSftp::SshSubsystemSftp(): _impl_ptr(new Impl())
{
    SIHD_UNEXPECTED_LOG(utils::init());
}

SshSubsystemSftp::~SshSubsystemSftp()
{
    // Close if the poll loop never did (e.g. server stopped before channel EOF)
    if (!_impl_ptr->closed)
        on_close();
    SIHD_UNEXPECTED_LOG(utils::finalize());
}

SshChannel *SshSubsystemSftp::channel() const
{
    return _impl_ptr->channel;
}

bool SshSubsystemSftp::is_running() const
{
    return _impl_ptr->running;
}

bool SshSubsystemSftp::forward_output()
{
    if (!_impl_ptr->running || !_impl_ptr->sftp)
        return false;
    const bool ready = _impl_ptr->ready_packets > 0;
    while (_impl_ptr->ready_packets > 0)
    {
        // a reply can show the data callback the buffer again: it recounts what is left
        --_impl_ptr->ready_packets;
        if (!_impl_ptr->initialized)
        {
            if (!_impl_ptr->do_init())
                return false;
            continue;
        }
        sftp_client_message msg = sftp_get_client_message(_impl_ptr->sftp);
        if (msg == nullptr)
            return false;
        _impl_ptr->handle_message(msg);
        sftp_client_message_free(msg);
    }
    return ready;
}

void SshSubsystemSftp::set_root_path(std::string_view path)
{
    _impl_ptr->root_path = path;
    if (!_impl_ptr->root_path.empty() && _impl_ptr->root_path.back() != '/')
        _impl_ptr->root_path += '/';
}

bool SshSubsystemSftp::on_start(SshChannel *channel, bool has_pty, [[maybe_unused]] const WinSize & winsize)
{
    _impl_ptr->channel = channel;

    if (has_pty)
    {
        SIHD_LOG(warning, "SshSubsystemSftp: SFTP does not use PTY");
    }

    ssh_channel ssh_chan = static_cast<ssh_channel>(channel->channel());
    if (!ssh_chan)
    {
        SIHD_LOG(error, "SshSubsystemSftp: invalid channel");
        return false;
    }

    ssh_session ssh_sess = ssh_channel_get_session(ssh_chan);
    if (!ssh_sess)
    {
        SIHD_LOG(error, "SshSubsystemSftp: cannot get SSH session from channel");
        return false;
    }

    _impl_ptr->sftp = sftp_server_new(ssh_sess, ssh_chan);
    if (!_impl_ptr->sftp)
    {
        SIHD_LOG(error, "SshSubsystemSftp: failed to create SFTP session");
        return false;
    }

    // sftp_server_init is NOT called here: it reads SSH_FXP_INIT from the client, deferred to do_init()

    _impl_ptr->running = true;
    _impl_ptr->initialized = false;

    SIHD_LOG(debug, "SshSubsystemSftp: started (waiting for SFTP init)");
    return true;
}

bool SshSubsystemSftp::Impl::do_init()
{
    if (initialized)
        return true;

    if (!sftp)
        return false;

    int rc = sftp_server_init(sftp);
    if (rc != SSH_OK)
    {
        SIHD_LOG(error, "SshSubsystemSftp: failed to init SFTP session: {}", sftp_get_error(sftp));
        return false;
    }

    initialized = true;
    SIHD_LOG(debug, "SshSubsystemSftp: SFTP initialized successfully");
    return true;
}

int SshSubsystemSftp::on_data(const void *data, size_t len)
{
    if (!_impl_ptr->running || !_impl_ptr->sftp)
        return -1;
    // the bytes stay in libssh for sftp_get_client_message: they are shown whole from its read position
    _impl_ptr->ready_packets = complete_packets(static_cast<const uint8_t *>(data), len);
    return 0;
}

void SshSubsystemSftp::Impl::handle_message(sftp_client_message_struct *msg)
{
    int type = sftp_client_message_get_type(msg);
    SIHD_LOG(debug, "SshSubsystemSftp: received message type {}", type);

    switch (type)
    {
        case SSH_FXP_REALPATH:
            this->handle_realpath(msg);
            break;
        case SSH_FXP_STAT:
            this->handle_stat(msg, true);
            break;
        case SSH_FXP_LSTAT:
            this->handle_stat(msg, false);
            break;
        case SSH_FXP_OPENDIR:
            this->handle_opendir(msg);
            break;
        case SSH_FXP_READDIR:
            this->handle_readdir(msg);
            break;
        case SSH_FXP_CLOSE:
            this->handle_close(msg);
            break;
        case SSH_FXP_MKDIR:
            this->handle_mkdir(msg);
            break;
        case SSH_FXP_RMDIR:
            this->handle_rmdir(msg);
            break;
        case SSH_FXP_OPEN:
            this->handle_open(msg);
            break;
        case SSH_FXP_READ:
            this->handle_read(msg);
            break;
        case SSH_FXP_WRITE:
            this->handle_write(msg);
            break;
        case SSH_FXP_REMOVE:
            this->handle_remove(msg);
            break;
        case SSH_FXP_RENAME:
            this->handle_rename(msg);
            break;
        case SSH_FXP_SETSTAT:
        case SSH_FXP_FSETSTAT:
            this->handle_setstat(msg);
            break;
        case SSH_FXP_READLINK:
            this->handle_readlink(msg);
            break;
        case SSH_FXP_SYMLINK:
            this->handle_symlink(msg);
            break;
        default:
            SIHD_LOG(warning, "SshSubsystemSftp: unsupported message type {}", type);
            this->reply_status(msg, SSH_FX_OP_UNSUPPORTED, "Operation not supported");
            break;
    }
}

void SshSubsystemSftp::on_resize([[maybe_unused]] const WinSize & winsize) {}

void SshSubsystemSftp::on_eof()
{
    SIHD_LOG(debug, "SshSubsystemSftp: EOF");
    _impl_ptr->running = false;
}

int SshSubsystemSftp::on_close()
{
    if (_impl_ptr->closed)
        return 0;
    _impl_ptr->closed = true;
    _impl_ptr->running = false;

    SIHD_LOG(debug, "SshSubsystemSftp: closing");

    _impl_ptr->handle_to_file.clear();
    _impl_ptr->handle_to_path.clear();
    _impl_ptr->dir_positions.clear();

    // Close the SSH channel ourselves since we manage channel I/O
    // Note: Do this BEFORE sftp_free() since sftp_free may invalidate the channel
    if (_impl_ptr->channel && _impl_ptr->channel->is_open())
    {
        SIHD_UNEXPECTED_LOG(_impl_ptr->channel->send_eof());
        SIHD_UNEXPECTED_LOG(_impl_ptr->channel->close());
    }

    if (_impl_ptr->sftp)
    {
        sftp_free(_impl_ptr->sftp);
        _impl_ptr->sftp = nullptr;
        // sftp_free freed the libssh channel: a next channel may get its address
        if (_impl_ptr->channel)
            _impl_ptr->channel->detach();
    }
    return 0;
}

void SshSubsystemSftp::Impl::handle_realpath(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "/";

    SIHD_LOG(debug, "SshSubsystemSftp: REALPATH '{}'", path);

    std::string resolved;
    if (path.empty() || path == ".")
    {
        resolved = "/";
    }
    else
    {
        const auto full = resolve_path(path);
        if (reply_if_refused(msg, full))
            return;
        std::string real_result = sihd::sys::fs::realpath(*full);
        if (real_result.empty() || root_path.empty())
        {
            resolved = real_result.empty() ? path : real_result;
        }
        else
        {
            // answer in the client view where the jail root is "/": host paths never leave the server
            const std::string root = resolve_path("").value_or(root_path);
            const std::string sep = sihd::sys::fs::sep_str();
            if (real_result == root)
                resolved = "/";
            else if (root == sep)
                resolved = real_result;
            else if (real_result.starts_with(root + sep))
            {
                resolved = real_result.substr(root.size());
                std::replace(resolved.begin(), resolved.end(), sihd::sys::fs::sep(), '/');
            }
            else
            {
                reply_status(msg, SSH_FX_PERMISSION_DENIED);
                return;
            }
        }
    }

    if (resolved.empty())
    {
        reply_status(msg, SSH_FX_NO_SUCH_FILE);
        return;
    }

    sftp_attributes attrs = create_sftp_attributes({resolved, resolved, 0, 0, 0, 0, 0, 0, SSH_FILEXFER_TYPE_REGULAR});
    if (attrs)
    {
        sftp_reply_name(msg, resolved.c_str(), attrs);
        sftp_attributes_free(attrs);
    }
    else
    {
        reply_status(msg, SSH_FX_FAILURE);
    }
}

void SshSubsystemSftp::Impl::handle_stat(sftp_client_message_struct *msg, bool follow_links)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSubsystemSftp: {} '{}'", follow_links ? "STAT" : "LSTAT", path);

    // windows has no links and the jail root no last component: nothing to keep unfollowed
    const bool names_root = sihd::sys::fs::normalize("/" + path) == "/";
    const bool follow = follow_links || names_root || build::is_windows;
    const auto resolved = follow ? resolve_path(path) : resolve_entry_path(path);
    if (reply_if_refused(msg, resolved))
        return;
    FileStat st;
    const int rc = stat_file(*resolved, st, follow_links);

    if (rc < 0)
    {
        auto err = Error::from_errno("could not stat '{}'", *resolved);
        reply_status(msg, sftp_status_from_error(err), err.message);
        return;
    }

    SftpAttributes attrs = attributes_from_stat(sihd::sys::fs::filename(path), st);
    sftp_attributes sa = create_sftp_attributes(attrs);
    if (sa)
    {
        sftp_reply_attr(msg, sa);
        sftp_attributes_free(sa);
    }
    else
    {
        reply_status(msg, SSH_FX_FAILURE);
    }
}

void SshSubsystemSftp::Impl::handle_opendir(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSubsystemSftp: OPENDIR '{}'", path);

    const auto resolved = resolve_path(path);
    if (reply_if_refused(msg, resolved))
        return;
    DIR *dir = opendir(resolved->c_str());
    if (!dir)
    {
        auto err = Error::from_errno("could not open directory '{}'", *resolved);
        reply_status(msg, sftp_status_from_error(err), err.message);
        return;
    }
    closedir(dir);

    std::string handle = generate_handle();
    handle_to_path[handle] = path;
    dir_positions[handle] = 0;

    ssh_string handle_str = ssh_string_from_char(handle.c_str());
    if (handle_str)
    {
        sftp_reply_handle(msg, handle_str);
        ssh_string_free(handle_str);
    }
    else
    {
        reply_status(msg, SSH_FX_FAILURE);
    }
}

void SshSubsystemSftp::Impl::handle_readdir(sftp_client_message_struct *msg)
{
    ssh_string handle_str = msg->handle;
    if (!handle_str)
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string handle = ssh_string_get_char(handle_str);
    auto it = handle_to_path.find(handle);
    if (it == handle_to_path.end())
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string path = it->second;
    SIHD_LOG(debug, "SshSubsystemSftp: READDIR '{}'", path);

    const auto resolved = resolve_path(path);
    if (reply_if_refused(msg, resolved))
        return;
    DIR *dir = opendir(resolved->c_str());
    if (!dir)
    {
        reply_status(msg, SSH_FX_NO_SUCH_FILE);
        return;
    }

    // the position counts directory entries read by the previous pages, listed or not
    const size_t first = dir_positions[handle];
    size_t position = 0;
    std::vector<SftpAttributes> entries;
    struct dirent *entry;
    while (entries.size() < max_readdir_entries && (entry = readdir(dir)) != nullptr)
    {
        if (position++ < first)
            continue;
        std::string full_path = *resolved + "/" + entry->d_name;
        FileStat st;
        if (stat_file(full_path, st, false) == 0)
        {
            entries.push_back(attributes_from_stat(entry->d_name, st));
        }
    }
    closedir(dir);
    dir_positions[handle] = position;

    if (entries.empty())
    {
        reply_status(msg, SSH_FX_EOF);
        return;
    }

    for (const auto & entry : entries)
    {
        sftp_attributes attrs = create_sftp_attributes(entry);
        if (attrs)
        {
            sftp_reply_names_add(msg, entry.name.c_str(), entry.longname.c_str(), attrs);
            sftp_attributes_free(attrs);
        }
    }

    sftp_reply_names(msg);
}

void SshSubsystemSftp::Impl::handle_close(sftp_client_message_struct *msg)
{
    ssh_string handle_str = msg->handle;
    if (!handle_str)
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string handle = ssh_string_get_char(handle_str);
    SIHD_LOG(debug, "SshSubsystemSftp: CLOSE handle={}", handle);

    auto fd_it = handle_to_file.find(handle);
    if (fd_it != handle_to_file.end())
    {
        handle_to_file.erase(fd_it);
    }

    handle_to_path.erase(handle);
    dir_positions.erase(handle);

    reply_status(msg, SSH_FX_OK);
}

void SshSubsystemSftp::Impl::handle_mkdir(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";
    sftp_attributes attrs = msg->attr;
    uint32_t mode = (attrs && (attrs->flags & SSH_FILEXFER_ATTR_PERMISSIONS)) ? attrs->permissions : 0755;

    SIHD_LOG(debug, "SshSubsystemSftp: MKDIR '{}' mode={:o}", path, mode);

    const auto resolved = resolve_path(path);
    if (reply_if_refused(msg, resolved))
        return;
    auto res = sihd::sys::fs::make_directory(*resolved, mode);
    if (!res)
    {
        reply_status(msg, sftp_status_from_error(res.error()), res.error().message);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSubsystemSftp::Impl::handle_rmdir(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSubsystemSftp: RMDIR '{}'", path);

    const auto resolved = resolve_entry_path(path);
    if (reply_if_refused(msg, resolved))
        return;
    auto res = sihd::sys::fs::remove_directory(*resolved);
    if (!res)
    {
        reply_status(msg, sftp_status_from_error(res.error()), res.error().message);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSubsystemSftp::Impl::handle_open(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";
    uint32_t flags = sftp_client_message_get_flags(msg);
    sftp_attributes attrs = msg->attr;
    uint32_t mode = (attrs && (attrs->flags & SSH_FILEXFER_ATTR_PERMISSIONS)) ? attrs->permissions : 0644;

    SIHD_LOG(debug, "SshSubsystemSftp: OPEN '{}' flags={:#x} mode={:o}", path, flags, mode);

    bool has_read = (flags & SSH_FXF_READ) != 0;
    bool has_write = (flags & SSH_FXF_WRITE) != 0;
    bool has_append = (flags & SSH_FXF_APPEND) != 0;
    bool has_trunc = (flags & SSH_FXF_TRUNC) != 0;
    bool has_creat = (flags & SSH_FXF_CREAT) != 0;
    bool has_excl = (flags & SSH_FXF_EXCL) != 0;

    const auto resolved = resolve_path(path);
    if (reply_if_refused(msg, resolved))
        return;
    // a request with neither access flag reads, as open(2) O_RDONLY did
    sihd::sys::File file;
    auto opened = file.open(*resolved,
                            {.read = has_read || !has_write,
                             .write = has_write,
                             .create = has_creat,
                             .truncate = has_trunc,
                             .exclusive = has_excl,
                             .append = has_append,
                             .permissions = mode});
    if (!opened)
    {
        reply_status(msg, sftp_status_from_error(opened.error()), opened.error().message);
        return;
    }

    // Apply permissions only when creating a new file
    if (has_creat && (attrs && (attrs->flags & SSH_FILEXFER_ATTR_PERMISSIONS)))
        SIHD_UNEXPECTED_LOG(sihd::sys::fs::permission_set(*resolved, mode));

    std::string handle = generate_handle();
    handle_to_path[handle] = path;
    handle_to_file.emplace(handle, std::move(file));

    ssh_string handle_str = ssh_string_from_char(handle.c_str());
    if (handle_str)
    {
        sftp_reply_handle(msg, handle_str);
        ssh_string_free(handle_str);
    }
    else
    {
        handle_to_file.erase(handle);
        reply_status(msg, SSH_FX_FAILURE);
    }
}

void SshSubsystemSftp::Impl::handle_read(sftp_client_message_struct *msg)
{
    ssh_string handle_str = msg->handle;
    if (!handle_str)
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string handle = ssh_string_get_char(handle_str);
    auto fd_it = handle_to_file.find(handle);
    if (fd_it == handle_to_file.end())
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    uint64_t offset = msg->offset;
    uint32_t len = std::min(msg->len, max_read_length);
    sihd::sys::File & file = fd_it->second;

    auto path_it = handle_to_path.find(handle);
    std::string path = path_it != handle_to_path.end() ? path_it->second : "";

    SIHD_LOG(debug, "SshSubsystemSftp: READ '{}' offset={} len={}", path, offset, len);

    if (SIHD_UNEXPECTED_LOG(file.seek_begin(static_cast<int64_t>(offset))))
    {
        reply_status(msg, SSH_FX_FAILURE);
        return;
    }

    std::vector<char> buf(len);
    auto read = file.read(buf.data(), len);
    if (SIHD_UNEXPECTED_LOG(read))
    {
        reply_status(msg, SSH_FX_FAILURE);
        return;
    }
    if (*read == 0)
    {
        reply_status(msg, SSH_FX_EOF);
        return;
    }

    sftp_reply_data(msg, buf.data(), static_cast<int>(*read));
}

void SshSubsystemSftp::Impl::handle_write(sftp_client_message_struct *msg)
{
    ssh_string handle_str = msg->handle;
    if (!handle_str)
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string handle = ssh_string_get_char(handle_str);
    auto fd_it = handle_to_file.find(handle);
    if (fd_it == handle_to_file.end())
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    // For WRITE messages, data and length are in msg->data (ssh_string), not msg->len
    // msg->len is only used for READ requests
    if (!msg->data)
    {
        reply_status(msg, SSH_FX_BAD_MESSAGE, "No data in write request");
        return;
    }

    uint64_t offset = msg->offset;
    const char *data = ssh_string_get_char(msg->data);
    uint32_t len = ssh_string_len(msg->data);
    sihd::sys::File & file = fd_it->second;

    auto path_it = handle_to_path.find(handle);
    std::string path = path_it != handle_to_path.end() ? path_it->second : "";

    SIHD_LOG(debug, "SshSubsystemSftp: WRITE '{}' offset={} len={}", path, offset, len);

    if (SIHD_UNEXPECTED_LOG(file.seek_begin(static_cast<int64_t>(offset))))
    {
        reply_status(msg, SSH_FX_FAILURE);
        return;
    }

    auto wrote = file.write(data, len);
    if (SIHD_UNEXPECTED_LOG(wrote) || *wrote != len)
    {
        if (wrote)
            SIHD_LOG(error, "SshSubsystemSftp: WRITE '{}' short write: {}/{}", path, *wrote, len);
        reply_status(msg, SSH_FX_FAILURE);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSubsystemSftp::Impl::handle_remove(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSubsystemSftp: REMOVE '{}'", path);

    const auto resolved = resolve_entry_path(path);
    if (reply_if_refused(msg, resolved))
        return;
    auto res = sihd::sys::fs::remove_file(*resolved);
    if (!res)
    {
        reply_status(msg, sftp_status_from_error(res.error()), res.error().message);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSubsystemSftp::Impl::handle_rename(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    const char *data = sftp_client_message_get_data(msg);
    std::string from = filename ? filename : "";
    std::string to = data ? data : "";

    SIHD_LOG(debug, "SshSubsystemSftp: RENAME '{}' -> '{}'", from, to);

    const auto resolved_from = resolve_entry_path(from);
    if (reply_if_refused(msg, resolved_from))
        return;
    const auto resolved_to = resolve_entry_path(to);
    if (reply_if_refused(msg, resolved_to))
        return;
    auto res = sihd::sys::fs::rename(*resolved_from, *resolved_to);
    if (!res)
    {
        reply_status(msg, sftp_status_from_error(res.error()), res.error().message);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSubsystemSftp::Impl::handle_setstat(sftp_client_message_struct *msg)
{
    std::string path;
    if (sftp_client_message_get_type(msg) == SSH_FXP_FSETSTAT)
    {
        // a handle, not a filename: an empty path would resolve to the jail root
        auto path_it = msg->handle ? handle_to_path.find(ssh_string_get_char(msg->handle)) : handle_to_path.end();
        if (path_it == handle_to_path.end())
        {
            reply_status(msg, SSH_FX_INVALID_HANDLE);
            return;
        }
        path = path_it->second;
    }
    else
    {
        const char *filename = sftp_client_message_get_filename(msg);
        path = filename ? filename : "";
    }
    sftp_attributes attrs = msg->attr;

    SIHD_LOG(debug, "SshSubsystemSftp: SETSTAT '{}'", path);

    if (!attrs)
    {
        reply_status(msg, SSH_FX_BAD_MESSAGE);
        return;
    }

    const auto resolved = resolve_path(path);
    if (reply_if_refused(msg, resolved))
        return;

    if (attrs->flags & SSH_FILEXFER_ATTR_PERMISSIONS)
    {
        if (auto res = sihd::sys::fs::permission_set(*resolved, attrs->permissions); !res)
        {
            reply_status(msg, sftp_status_from_error(res.error()), res.error().message);
            return;
        }
    }

    if (attrs->flags & SSH_FILEXFER_ATTR_UIDGID)
    {
#if !defined(__SIHD_WINDOWS__)
        if (chown(resolved->c_str(), attrs->uid, attrs->gid) < 0)
        {
            reply_status(msg, SSH_FX_PERMISSION_DENIED);
            return;
        }
#endif
    }

    if (attrs->flags & SSH_FILEXFER_ATTR_SIZE)
    {
        if (auto res = sihd::sys::fs::truncate(*resolved, attrs->size); !res)
        {
            reply_status(msg, sftp_status_from_error(res.error()), res.error().message);
            return;
        }
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSubsystemSftp::Impl::handle_readlink(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSubsystemSftp: READLINK '{}'", path);

#if defined(__SIHD_WINDOWS__)
    (void)path;
    reply_status(msg, SSH_FX_OP_UNSUPPORTED, "Symlinks not supported on Windows");
#else
    const auto resolved = resolve_entry_path(path);
    if (reply_if_refused(msg, resolved))
        return;
    std::array<char, PATH_MAX> buf;
    ssize_t len = ::readlink(resolved->c_str(), buf.data(), buf.size() - 1);
    if (len <= 0)
    {
        reply_status(msg, SSH_FX_NO_SUCH_FILE);
        return;
    }

    std::string target(buf.data(), static_cast<size_t>(len));

    sftp_attributes attrs = create_sftp_attributes({target, target, 0, 0, 0, 0, 0, 0, SSH_FILEXFER_TYPE_SYMLINK});
    if (attrs)
    {
        sftp_reply_name(msg, target.c_str(), attrs);
        sftp_attributes_free(attrs);
    }
    else
    {
        reply_status(msg, SSH_FX_FAILURE);
    }
#endif
}

void SshSubsystemSftp::Impl::handle_symlink(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    const char *data = sftp_client_message_get_data(msg);
    std::string target = filename ? filename : "";
    std::string link = data ? data : "";

    SIHD_LOG(debug, "SshSubsystemSftp: SYMLINK '{}' -> '{}'", link, target);

#if defined(__SIHD_WINDOWS__)
    (void)target;
    (void)link;
    reply_status(msg, SSH_FX_OP_UNSUPPORTED, "Symlinks not supported on Windows");
#else
    const auto resolved_link = resolve_entry_path(link);
    if (reply_if_refused(msg, resolved_link))
        return;
    if (::symlink(target.c_str(), resolved_link->c_str()) < 0)
    {
        uint32_t error = (errno == EEXIST) ? SSH_FX_FILE_ALREADY_EXISTS : SSH_FX_PERMISSION_DENIED;
        reply_status(msg, error);
        return;
    }

    reply_status(msg, SSH_FX_OK);
#endif
}

Result<std::string> SshSubsystemSftp::Impl::resolve_path(const std::string & path)
{
    return sihd::sys::fs::jail(root_path, path);
}

// the entry itself, its last component not followed (rm, rename, lstat... act on a link, not its target)
Result<std::string> SshSubsystemSftp::Impl::resolve_entry_path(const std::string & path)
{
    if (root_path.empty())
        return this->resolve_path(path);

    const std::string clean = sihd::sys::fs::normalize("/" + path);
    const std::string name = sihd::sys::fs::filename(clean);
    Result<std::string> entry;
    // windows refuses symbolic links: no last component to keep unfollowed
    if (build::is_windows || name.empty() || name == "..")
        entry = this->resolve_path(path);
    else
    {
        auto parent = this->resolve_path(sihd::sys::fs::parent(clean));
        SIHD_UNEXPECTED_RETURN(parent);
        entry = sihd::sys::fs::combine(*parent, name);
    }
    SIHD_UNEXPECTED_RETURN(entry);

    const auto root = this->resolve_path("");
    if (root && *entry == *root)
        return std::unexpected(Error(permission_denied, "'{}' names the jail root", path));
    return entry;
}

bool SshSubsystemSftp::Impl::reply_if_refused(sftp_client_message_struct *msg, const Result<std::string> & resolved)
{
    if (resolved)
        return false;
    reply_status(msg, sftp_status_from_error(resolved.error()), resolved.error().message);
    return true;
}

std::string SshSubsystemSftp::Impl::generate_handle()
{
    return fmt::format("sftp_handle_{}", next_handle_id++);
}

void SshSubsystemSftp::Impl::reply_status(sftp_client_message_struct *msg, uint32_t status, const std::string & message)
{
    const char *msg_str = message.empty() ? nullptr : message.c_str();
    sftp_reply_status(msg, status, msg_str);
}

int SshSubsystemSftp::stdout_fd() const
{
    return -1;
}

int SshSubsystemSftp::stderr_fd() const
{
    return -1;
}

bool SshSubsystemSftp::manages_channel_io() const
{
    return true;
}

} // namespace sihd::ssh
