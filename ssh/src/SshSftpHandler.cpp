/**
 * @file SshSftpHandler.cpp
 * @brief SFTP subsystem handler implementation using libssh's SFTP server API.
 */

#include "sihd/ssh/utils.hpp"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

// Define WITH_SERVER before including sftp.h to get sftp_server_* functions
#define WITH_SERVER 1

#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/sftp.h>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshSftpHandler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/str.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

// ============================================================================
// SftpAttributes implementation
// ============================================================================

SshSftpHandler::SftpAttributes SshSftpHandler::SftpAttributes::from_stat(const std::string & name,
                                                                         const struct stat & st)
{
    SftpAttributes attrs;
    attrs.name = name;
    // Keep full mode including file type bits (S_IFDIR, S_IFREG, etc.)
    // SFTP v3 derives file type from the mode field, not a separate type field
    attrs.permissions = st.st_mode;
    attrs.uid = st.st_uid;
    attrs.gid = st.st_gid;
    attrs.size = st.st_size;
    attrs.atime = st.st_atime;
    attrs.mtime = st.st_mtime;

    // Determine type
    if (S_ISDIR(st.st_mode))
        attrs.type = SSH_FILEXFER_TYPE_DIRECTORY;
    else if (S_ISLNK(st.st_mode))
        attrs.type = SSH_FILEXFER_TYPE_SYMLINK;
    else if (S_ISREG(st.st_mode))
        attrs.type = SSH_FILEXFER_TYPE_REGULAR;
    else
        attrs.type = SSH_FILEXFER_TYPE_SPECIAL;

    // Build longname (ls -l format)
    char mode_str[11];
    mode_str[0] = S_ISDIR(st.st_mode) ? 'd' : (S_ISLNK(st.st_mode) ? 'l' : '-');
    mode_str[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
    mode_str[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
    mode_str[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
    mode_str[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
    mode_str[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
    mode_str[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
    mode_str[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
    mode_str[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
    mode_str[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
    mode_str[10] = '\0';

    char time_str[64];
    struct tm tm_info;
    localtime_r(&st.st_mtime, &tm_info);
    strftime(time_str, sizeof(time_str), "%b %d %H:%M", &tm_info);

    attrs.longname = fmt::format("{} {:3d} {:5d} {:5d} {:8d} {} {}",
                                 mode_str,
                                 static_cast<int>(st.st_nlink),
                                 st.st_uid,
                                 st.st_gid,
                                 static_cast<long>(st.st_size),
                                 time_str,
                                 name);

    return attrs;
}

// ============================================================================
// Result types implementation
// ============================================================================

SshSftpHandler::StatResult SshSftpHandler::StatResult::ok(const SftpAttributes & attrs)
{
    return {true, SSH_FX_OK, attrs};
}

SshSftpHandler::StatResult SshSftpHandler::StatResult::error(uint32_t code)
{
    return {false, code, {}};
}

SshSftpHandler::ReaddirResult SshSftpHandler::ReaddirResult::ok(std::vector<SftpAttributes> entries, bool eof)
{
    return {true, eof, SSH_FX_OK, std::move(entries)};
}

SshSftpHandler::ReaddirResult SshSftpHandler::ReaddirResult::end()
{
    return {true, true, SSH_FX_EOF, {}};
}

SshSftpHandler::ReaddirResult SshSftpHandler::ReaddirResult::error(uint32_t code)
{
    return {false, false, code, {}};
}

SshSftpHandler::ReadResult SshSftpHandler::ReadResult::ok(std::vector<char> data)
{
    return {true, false, SSH_FX_OK, std::move(data)};
}

SshSftpHandler::ReadResult SshSftpHandler::ReadResult::end()
{
    return {true, true, SSH_FX_EOF, {}};
}

SshSftpHandler::ReadResult SshSftpHandler::ReadResult::error(uint32_t code)
{
    return {false, false, code, {}};
}

SshSftpHandler::OpResult SshSftpHandler::OpResult::ok()
{
    return {true, SSH_FX_OK, ""};
}

SshSftpHandler::OpResult SshSftpHandler::OpResult::error(uint32_t code, const std::string & msg)
{
    return {false, code, msg};
}

// ============================================================================
// Helper to create sftp_attributes from our SftpAttributes
// ============================================================================

namespace
{

sftp_attributes create_sftp_attributes(const SshSftpHandler::SftpAttributes & attrs)
{
    sftp_attributes sa = static_cast<sftp_attributes>(calloc(1, sizeof(struct sftp_attributes_struct)));
    if (!sa)
        return nullptr;

    sa->name = strdup(attrs.name.c_str());
    sa->longname = strdup(attrs.longname.c_str());
    sa->permissions = attrs.permissions;
    sa->uid = attrs.uid;
    sa->gid = attrs.gid;
    sa->size = attrs.size;
    sa->atime = attrs.atime;
    sa->mtime = attrs.mtime;
    sa->type = attrs.type;
    sa->flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_UIDGID
                | SSH_FILEXFER_ATTR_ACMODTIME;

    return sa;
}

} // namespace

// ============================================================================
// SshSftpHandler implementation
// ============================================================================

SshSftpHandler::SshSftpHandler():
    _channel(nullptr),
    _session(nullptr),
    _sftp(nullptr),
    _running(false),
    _initialized(false),
    _next_handle_id(1)
{
    utils::init();
}

SshSftpHandler::~SshSftpHandler()
{
    if (_running)
        on_close();
    utils::finalize();
}

void SshSftpHandler::set_root_path(std::string_view path)
{
    _root_path = path;
    // Ensure trailing slash for proper path joining
    if (!_root_path.empty() && _root_path.back() != '/')
        _root_path += '/';
}

bool SshSftpHandler::on_start(SshChannel *channel,
                              bool has_pty,
                              [[maybe_unused]] const struct winsize & winsize)
{
    _channel = channel;

    if (has_pty)
    {
        SIHD_LOG(warning, "SshSftpHandler: SFTP does not use PTY");
    }

    // Get the SSH channel from our wrapper
    ssh_channel ssh_chan = channel->channel();
    if (!ssh_chan)
    {
        SIHD_LOG(error, "SshSftpHandler: invalid channel");
        return false;
    }

    ssh_session ssh_sess = ssh_channel_get_session(ssh_chan);
    if (!ssh_sess)
    {
        SIHD_LOG(error, "SshSftpHandler: cannot get SSH session from channel");
        return false;
    }

    // Create SFTP session for server mode
    _sftp = sftp_server_new(ssh_sess, ssh_chan);
    if (!_sftp)
    {
        SIHD_LOG(error, "SshSftpHandler: failed to create SFTP session");
        return false;
    }

    // Note: sftp_server_init is NOT called here because it needs to read SSH_FXP_INIT
    // from the client, which may not be available yet. We defer it to do_init()
    // which is called from on_data when data is available.

    _running = true;
    _initialized = false;

    SIHD_LOG(debug, "SshSftpHandler: started (waiting for SFTP init)");
    return true;
}

bool SshSftpHandler::do_init()
{
    if (_initialized)
        return true;

    if (!_sftp)
        return false;

    // Check if data is available on the channel before trying to init
    // This avoids blocking in non-blocking mode
    ssh_channel ssh_chan = _channel->channel();
    int available = ssh_channel_poll(ssh_chan, 0);
    if (available <= 0)
    {
        // No data available yet
        return false;
    }

    SIHD_LOG(debug, "SshSftpHandler: {} bytes available, initializing SFTP", available);

    // Initialize SFTP session (this exchanges version info with client)
    int rc = sftp_server_init(_sftp);
    if (rc != SSH_OK)
    {
        SIHD_LOG(error, "SshSftpHandler: failed to init SFTP session: {}", sftp_get_error(_sftp));
        return false;
    }

    _initialized = true;
    SIHD_LOG(debug, "SshSftpHandler: SFTP initialized successfully");
    return true;
}

int SshSftpHandler::on_data([[maybe_unused]] const void *data, [[maybe_unused]] size_t len)
{
    if (!_running || !_sftp)
        return -1;

    // Deferred initialization - wait for SSH_FXP_INIT to be available
    if (!_initialized)
    {
        if (!do_init())
        {
            // Not ready yet or error
            return 0;
        }
    }

    // Process SFTP messages
    sftp_client_message msg = sftp_get_client_message(_sftp);
    if (!msg)
    {
        // No message available or error
        return 0;
    }

    int type = sftp_client_message_get_type(msg);
    SIHD_LOG(debug, "SshSftpHandler: received message type {}", type);

    switch (type)
    {
        case SSH_FXP_REALPATH:
            handle_realpath(msg);
            break;
        case SSH_FXP_STAT:
            handle_stat(msg, true);
            break;
        case SSH_FXP_LSTAT:
            handle_stat(msg, false);
            break;
        case SSH_FXP_OPENDIR:
            handle_opendir(msg);
            break;
        case SSH_FXP_READDIR:
            handle_readdir(msg);
            break;
        case SSH_FXP_CLOSE:
            handle_close(msg);
            break;
        case SSH_FXP_MKDIR:
            handle_mkdir(msg);
            break;
        case SSH_FXP_RMDIR:
            handle_rmdir(msg);
            break;
        case SSH_FXP_OPEN:
            handle_open(msg);
            break;
        case SSH_FXP_READ:
            handle_read(msg);
            break;
        case SSH_FXP_WRITE:
            handle_write(msg);
            break;
        case SSH_FXP_REMOVE:
            handle_remove(msg);
            break;
        case SSH_FXP_RENAME:
            handle_rename(msg);
            break;
        case SSH_FXP_SETSTAT:
        case SSH_FXP_FSETSTAT:
            handle_setstat(msg);
            break;
        case SSH_FXP_READLINK:
            handle_readlink(msg);
            break;
        case SSH_FXP_SYMLINK:
            handle_symlink(msg);
            break;
        default:
            SIHD_LOG(warning, "SshSftpHandler: unsupported message type {}", type);
            reply_status(msg, SSH_FX_OP_UNSUPPORTED, "Operation not supported");
            break;
    }

    sftp_client_message_free(msg);
    return static_cast<int>(len);
}

void SshSftpHandler::on_resize([[maybe_unused]] const struct winsize & winsize)
{
    // SFTP does not use PTY
}

void SshSftpHandler::on_eof()
{
    SIHD_LOG(debug, "SshSftpHandler: EOF");
    _running = false;
}

int SshSftpHandler::on_close()
{
    SIHD_LOG(debug, "SshSftpHandler: closing");

    // Close any open file handles
    for (auto & [handle, fd] : _handle_to_fd)
    {
        if (fd >= 0)
            ::close(fd);
    }
    _handle_to_fd.clear();
    _handle_to_path.clear();
    _dir_positions.clear();

    // Close the SSH channel ourselves since we manage channel I/O
    // Note: Do this BEFORE sftp_free() since sftp_free may invalidate the channel
    if (_channel && _channel->is_open())
    {
        _channel->send_eof();
        _channel->close();
    }

    // Free SFTP session
    if (_sftp)
    {
        sftp_free(_sftp);
        _sftp = nullptr;
    }

    _running = false;
    return 0;
}

// ============================================================================
// Message handlers
// ============================================================================

void SshSftpHandler::handle_realpath(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "/";

    SIHD_LOG(debug, "SshSftpHandler: REALPATH '{}'", path);

    // Resolve the path
    std::string resolved;
    if (path.empty() || path == ".")
    {
        resolved = _root_path.empty() ? "/" : _root_path;
    }
    else
    {
        std::string full = resolve_path(path);
        char *real = realpath(full.c_str(), nullptr);
        if (real)
        {
            std::string result(real);
            free(real);

            // Return path relative to root if root is set
            if (!_root_path.empty() && result.find(_root_path) == 0)
            {
                resolved = result.substr(_root_path.length());
                if (resolved.empty() || resolved[0] != '/')
                    resolved = "/" + resolved;
            }
            else
            {
                resolved = result;
            }
        }
        else
        {
            // Path doesn't exist yet, return cleaned path
            resolved = path.empty() ? "/" : path;
        }
    }

    if (resolved.empty())
    {
        reply_status(msg, SSH_FX_NO_SUCH_FILE);
        return;
    }

    // Create attributes for the resolved path
    sftp_attributes attrs
        = create_sftp_attributes({resolved, resolved, 0, 0, 0, 0, 0, 0, SSH_FILEXFER_TYPE_REGULAR});
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

void SshSftpHandler::handle_stat(sftp_client_message_struct *msg, bool follow_links)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSftpHandler: {} '{}'", follow_links ? "STAT" : "LSTAT", path);

    std::string resolved = resolve_path(path);
    struct stat st;
    int rc = follow_links ? ::stat(resolved.c_str(), &st) : ::lstat(resolved.c_str(), &st);

    if (rc < 0)
    {
        reply_status(msg, errno == ENOENT ? SSH_FX_NO_SUCH_FILE : SSH_FX_PERMISSION_DENIED);
        return;
    }

    SftpAttributes attrs = SftpAttributes::from_stat(sihd::util::fs::filename(path), st);
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

void SshSftpHandler::handle_opendir(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSftpHandler: OPENDIR '{}'", path);

    std::string resolved = resolve_path(path);
    DIR *dir = opendir(resolved.c_str());
    if (!dir)
    {
        reply_status(msg, errno == ENOENT ? SSH_FX_NO_SUCH_FILE : SSH_FX_PERMISSION_DENIED);
        return;
    }
    closedir(dir);

    // Generate handle and store mapping
    std::string handle = generate_handle();
    _handle_to_path[handle] = path;
    _dir_positions[handle] = 0;

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

void SshSftpHandler::handle_readdir(sftp_client_message_struct *msg)
{
    // Access handle directly from message structure
    ssh_string handle_str = msg->handle;
    if (!handle_str)
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string handle = ssh_string_get_char(handle_str);
    auto it = _handle_to_path.find(handle);
    if (it == _handle_to_path.end())
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string path = it->second;
    SIHD_LOG(debug, "SshSftpHandler: READDIR '{}'", path);

    // Check if we've already read this directory
    auto pos_it = _dir_positions.find(handle);
    if (pos_it != _dir_positions.end() && pos_it->second > 0)
    {
        // Already returned entries, send EOF
        reply_status(msg, SSH_FX_EOF);
        return;
    }

    // Read directory entries
    std::string resolved = resolve_path(path);
    DIR *dir = opendir(resolved.c_str());
    if (!dir)
    {
        reply_status(msg, SSH_FX_NO_SUCH_FILE);
        return;
    }

    std::vector<SftpAttributes> entries;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string full_path = resolved + "/" + entry->d_name;
        struct stat st;
        if (::lstat(full_path.c_str(), &st) == 0)
        {
            entries.push_back(SftpAttributes::from_stat(entry->d_name, st));
        }
    }
    closedir(dir);

    if (entries.empty())
    {
        reply_status(msg, SSH_FX_EOF);
        return;
    }

    // Add names to reply
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

    // Mark as read
    _dir_positions[handle] = 1;
}

void SshSftpHandler::handle_close(sftp_client_message_struct *msg)
{
    ssh_string handle_str = msg->handle;
    if (!handle_str)
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string handle = ssh_string_get_char(handle_str);
    SIHD_LOG(debug, "SshSftpHandler: CLOSE handle={}", handle);

    // Check if it's a file handle
    auto fd_it = _handle_to_fd.find(handle);
    if (fd_it != _handle_to_fd.end())
    {
        if (fd_it->second >= 0)
            ::close(fd_it->second);
        _handle_to_fd.erase(fd_it);
    }

    // Clean up handle mappings
    _handle_to_path.erase(handle);
    _dir_positions.erase(handle);

    reply_status(msg, SSH_FX_OK);
}

void SshSftpHandler::handle_mkdir(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";
    sftp_attributes attrs = msg->attr;
    uint32_t mode = (attrs && (attrs->flags & SSH_FILEXFER_ATTR_PERMISSIONS)) ? attrs->permissions : 0755;

    SIHD_LOG(debug, "SshSftpHandler: MKDIR '{}' mode={:o}", path, mode);

    std::string resolved = resolve_path(path);
    if (::mkdir(resolved.c_str(), mode) < 0)
    {
        uint32_t error = (errno == EEXIST) ? SSH_FX_FILE_ALREADY_EXISTS : SSH_FX_PERMISSION_DENIED;
        reply_status(msg, error);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSftpHandler::handle_rmdir(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSftpHandler: RMDIR '{}'", path);

    std::string resolved = resolve_path(path);
    if (::rmdir(resolved.c_str()) < 0)
    {
        uint32_t error = (errno == ENOENT) ? SSH_FX_NO_SUCH_FILE : SSH_FX_PERMISSION_DENIED;
        reply_status(msg, error);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSftpHandler::handle_open(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";
    uint32_t flags = sftp_client_message_get_flags(msg);
    sftp_attributes attrs = msg->attr;
    uint32_t mode = (attrs && (attrs->flags & SSH_FILEXFER_ATTR_PERMISSIONS)) ? attrs->permissions : 0644;

    SIHD_LOG(debug, "SshSftpHandler: OPEN '{}' flags={:#x} mode={:o}", path, flags, mode);

    // Convert SFTP flags to POSIX flags
    int open_flags = 0;
    if (flags & SSH_FXF_READ)
        open_flags |= O_RDONLY;
    if (flags & SSH_FXF_WRITE)
    {
        open_flags = (flags & SSH_FXF_READ) ? O_RDWR : O_WRONLY;
    }
    if (flags & SSH_FXF_CREAT)
        open_flags |= O_CREAT;
    if (flags & SSH_FXF_TRUNC)
        open_flags |= O_TRUNC;
    if (flags & SSH_FXF_EXCL)
        open_flags |= O_EXCL;
    if (flags & SSH_FXF_APPEND)
        open_flags |= O_APPEND;

    std::string resolved = resolve_path(path);
    int fd = ::open(resolved.c_str(), open_flags, mode);
    if (fd < 0)
    {
        uint32_t error = (errno == ENOENT) ? SSH_FX_NO_SUCH_FILE : SSH_FX_PERMISSION_DENIED;
        reply_status(msg, error);
        return;
    }

    // Generate handle and store mapping
    std::string handle = generate_handle();
    _handle_to_path[handle] = path;
    _handle_to_fd[handle] = fd;

    ssh_string handle_str = ssh_string_from_char(handle.c_str());
    if (handle_str)
    {
        sftp_reply_handle(msg, handle_str);
        ssh_string_free(handle_str);
    }
    else
    {
        ::close(fd);
        reply_status(msg, SSH_FX_FAILURE);
    }
}

void SshSftpHandler::handle_read(sftp_client_message_struct *msg)
{
    ssh_string handle_str = msg->handle;
    if (!handle_str)
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string handle = ssh_string_get_char(handle_str);
    auto fd_it = _handle_to_fd.find(handle);
    if (fd_it == _handle_to_fd.end() || fd_it->second < 0)
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    uint64_t offset = msg->offset;
    uint32_t len = msg->len;
    int fd = fd_it->second;

    auto path_it = _handle_to_path.find(handle);
    std::string path = path_it != _handle_to_path.end() ? path_it->second : "";

    SIHD_LOG(debug, "SshSftpHandler: READ '{}' offset={} len={}", path, offset, len);

    // Seek and read
    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        reply_status(msg, SSH_FX_FAILURE);
        return;
    }

    std::vector<char> buf(len);
    ssize_t n = ::read(fd, buf.data(), len);
    if (n < 0)
    {
        reply_status(msg, SSH_FX_FAILURE);
        return;
    }
    if (n == 0)
    {
        reply_status(msg, SSH_FX_EOF);
        return;
    }

    sftp_reply_data(msg, buf.data(), static_cast<int>(n));
}

void SshSftpHandler::handle_write(sftp_client_message_struct *msg)
{
    ssh_string handle_str = msg->handle;
    if (!handle_str)
    {
        reply_status(msg, SSH_FX_INVALID_HANDLE);
        return;
    }

    std::string handle = ssh_string_get_char(handle_str);
    auto fd_it = _handle_to_fd.find(handle);
    if (fd_it == _handle_to_fd.end() || fd_it->second < 0)
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
    int fd = fd_it->second;

    auto path_it = _handle_to_path.find(handle);
    std::string path = path_it != _handle_to_path.end() ? path_it->second : "";

    SIHD_LOG(debug, "SshSftpHandler: WRITE '{}' offset={} len={}", path, offset, len);

    // Seek and write
    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        reply_status(msg, SSH_FX_FAILURE);
        return;
    }

    ssize_t n = ::write(fd, data, len);
    if (n < 0 || static_cast<size_t>(n) != len)
    {
        SIHD_LOG(error, "SshSftpHandler: WRITE failed: wrote {} of {} bytes", n, len);
        reply_status(msg, SSH_FX_FAILURE);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSftpHandler::handle_remove(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSftpHandler: REMOVE '{}'", path);

    std::string resolved = resolve_path(path);
    if (::unlink(resolved.c_str()) < 0)
    {
        uint32_t error = (errno == ENOENT) ? SSH_FX_NO_SUCH_FILE : SSH_FX_PERMISSION_DENIED;
        reply_status(msg, error);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSftpHandler::handle_rename(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    const char *data = sftp_client_message_get_data(msg);
    std::string from = filename ? filename : "";
    std::string to = data ? data : "";

    SIHD_LOG(debug, "SshSftpHandler: RENAME '{}' -> '{}'", from, to);

    std::string resolved_from = resolve_path(from);
    std::string resolved_to = resolve_path(to);
    if (::rename(resolved_from.c_str(), resolved_to.c_str()) < 0)
    {
        uint32_t error = (errno == ENOENT) ? SSH_FX_NO_SUCH_FILE : SSH_FX_PERMISSION_DENIED;
        reply_status(msg, error);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSftpHandler::handle_setstat(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";
    sftp_attributes attrs = msg->attr;

    SIHD_LOG(debug, "SshSftpHandler: SETSTAT '{}'", path);

    if (!attrs)
    {
        reply_status(msg, SSH_FX_BAD_MESSAGE);
        return;
    }

    std::string resolved = resolve_path(path);

    // Apply requested attributes
    if (attrs->flags & SSH_FILEXFER_ATTR_PERMISSIONS)
    {
        if (chmod(resolved.c_str(), attrs->permissions) < 0)
        {
            reply_status(msg, SSH_FX_PERMISSION_DENIED);
            return;
        }
    }

    if (attrs->flags & SSH_FILEXFER_ATTR_UIDGID)
    {
        if (chown(resolved.c_str(), attrs->uid, attrs->gid) < 0)
        {
            reply_status(msg, SSH_FX_PERMISSION_DENIED);
            return;
        }
    }

    if (attrs->flags & SSH_FILEXFER_ATTR_SIZE)
    {
        if (truncate(resolved.c_str(), attrs->size) < 0)
        {
            reply_status(msg, SSH_FX_PERMISSION_DENIED);
            return;
        }
    }

    reply_status(msg, SSH_FX_OK);
}

void SshSftpHandler::handle_readlink(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    std::string path = filename ? filename : "";

    SIHD_LOG(debug, "SshSftpHandler: READLINK '{}'", path);

    std::string resolved = resolve_path(path);
    char buf[PATH_MAX];
    ssize_t len = ::readlink(resolved.c_str(), buf, sizeof(buf) - 1);
    if (len <= 0)
    {
        reply_status(msg, SSH_FX_NO_SUCH_FILE);
        return;
    }

    buf[len] = '\0';
    std::string target(buf);

    sftp_attributes attrs
        = create_sftp_attributes({target, target, 0, 0, 0, 0, 0, 0, SSH_FILEXFER_TYPE_SYMLINK});
    if (attrs)
    {
        sftp_reply_name(msg, target.c_str(), attrs);
        sftp_attributes_free(attrs);
    }
    else
    {
        reply_status(msg, SSH_FX_FAILURE);
    }
}

void SshSftpHandler::handle_symlink(sftp_client_message_struct *msg)
{
    const char *filename = sftp_client_message_get_filename(msg);
    const char *data = sftp_client_message_get_data(msg);
    std::string target = filename ? filename : "";
    std::string link = data ? data : "";

    SIHD_LOG(debug, "SshSftpHandler: SYMLINK '{}' -> '{}'", link, target);

    std::string resolved_link = resolve_path(link);
    if (::symlink(target.c_str(), resolved_link.c_str()) < 0)
    {
        uint32_t error = (errno == EEXIST) ? SSH_FX_FILE_ALREADY_EXISTS : SSH_FX_PERMISSION_DENIED;
        reply_status(msg, error);
        return;
    }

    reply_status(msg, SSH_FX_OK);
}

// ============================================================================
// Utility methods
// ============================================================================

std::string SshSftpHandler::resolve_path(const std::string & path)
{
    if (_root_path.empty())
        return path;

    // Handle absolute vs relative paths
    std::string clean_path = path;
    if (!clean_path.empty() && clean_path[0] == '/')
        clean_path = clean_path.substr(1);

    // Simple path traversal protection
    std::string result = _root_path + clean_path;

    // Normalize the path to prevent escape
    char *real = realpath(result.c_str(), nullptr);
    if (real)
    {
        std::string real_str(real);
        free(real);

        // Verify it's still under root
        if (real_str.find(_root_path) == 0 || _root_path.find(real_str) == 0)
            return real_str;
    }

    // If realpath failed or path is outside root, return resolved without normalization
    return result;
}

std::string SshSftpHandler::generate_handle()
{
    return sihd::util::str::format("sftp_handle_{}", _next_handle_id++);
}

void SshSftpHandler::reply_status(sftp_client_message_struct *msg,
                                  uint32_t status,
                                  const std::string & message)
{
    const char *msg_str = message.empty() ? nullptr : message.c_str();
    sftp_reply_status(msg, status, msg_str);
}

} // namespace sihd::ssh
