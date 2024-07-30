#include <fcntl.h>

#include <cstring>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include <sihd/util/File.hpp>
#include <sihd/util/Logger.hpp>

#include <sihd/ssh/Sftp.hpp>

#ifndef SIHD_SSH_SFTP_BUFSIZE
# define SIHD_SSH_SFTP_BUFSIZE 4096
#endif

namespace sihd::ssh
{

namespace
{

struct SftpFileDeleter
{
        void operator()(sftp_file_struct *ptr)
        {
            // TODO log error if ret != SSH_NO_ERROR
            if (ptr != nullptr)
                sftp_close(ptr);
        }
};

using SftpFile = std::unique_ptr<sftp_file_struct, SftpFileDeleter>;

struct SftpDirDeleter
{
        void operator()(sftp_dir_struct *ptr)
        {
            // TODO log error if ret != SSH_NO_ERROR
            if (ptr != nullptr)
                sftp_closedir(ptr);
        }
};

using SftpDir = std::unique_ptr<sftp_dir_struct, SftpDirDeleter>;

} // namespace

SIHD_LOGGER;

Sftp::Sftp(ssh_session_struct *session): _ssh_session_ptr(session), _sftp_session_ptr(nullptr) {}

Sftp::~Sftp()
{
    this->close();
}

bool Sftp::open()
{
    _sftp_session_ptr = sftp_new(_ssh_session_ptr);
    if (_sftp_session_ptr == nullptr)
    {
        SIHD_LOG(error, "Sftp: failed to create sftp session: {}", ssh_get_error(_ssh_session_ptr));
        return false;
    }
    int r = sftp_init(_sftp_session_ptr);
    if (r != SSH_FX_OK)
    {
        SIHD_LOG(error, "Sftp: failed to initialize sftp session: {}", this->error());
        this->close();
        return false;
    }
    return true;
}

void Sftp::close()
{
    if (_sftp_session_ptr != nullptr)
    {
        sftp_free(_sftp_session_ptr);
        _sftp_session_ptr = nullptr;
    }
}

bool Sftp::send_file(std::string_view local_path, std::string_view remote_path, mode_t mode)
{
    sihd::util::File local_file(local_path, "rb");
    if (local_file.is_open() == false)
        return false;

    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    SftpFile remote_file(sftp_open(_sftp_session_ptr, remote_path.data(), flags, mode));
    if (remote_file.get() == nullptr)
    {
        SIHD_LOG(error, "Sftp: failed to open remote file: '{}' {}", remote_path, ssh_get_error(_ssh_session_ptr));
        return false;
    }
    bool ret = true;
    char buf[SIHD_SSH_SFTP_BUFSIZE + 1];
    ssize_t nread;
    int nwritten;
    while (local_file.eof() == false && local_file.error() == false)
    {
        nread = local_file.read(buf, SIHD_SSH_SFTP_BUFSIZE);
        if (nread < 0)
        {
            SIHD_LOG(error, "Sftp: error reading local file: {}", local_path);
            ret = false;
            break;
        }
        nwritten = sftp_write(remote_file.get(), buf, nread);
        if (nwritten != nread)
        {
            SIHD_LOG_ERROR("Sftp: failed writing remote file: '{}' '{} != '{}'", remote_path, nwritten, nread);
            ret = false;
            break;
        }
    }
    return ret;
}

bool Sftp::get_file(std::string_view remote_path, std::string_view local_path)
{
    sihd::util::File local_file(local_path, "wb");
    if (local_file.is_open() == false)
        return false;

    int flags = O_RDONLY;
    SftpFile remote_file(sftp_open(_sftp_session_ptr, remote_path.data(), flags, 0));
    if (remote_file.get() == nullptr)
    {
        SIHD_LOG(error, "Sftp: failed to open remote file: '{}' {}", remote_path, ssh_get_error(_ssh_session_ptr));
        return false;
    }
    bool ret = true;
    char buf[SIHD_SSH_SFTP_BUFSIZE + 1];
    ssize_t nread;
    int nwritten;
    while (true)
    {
        nread = sftp_read(remote_file.get(), buf, SIHD_SSH_SFTP_BUFSIZE);
        if (nread < 0)
        {
            SIHD_LOG(error, "Sftp: error reading remote file: {}", remote_path);
            ret = false;
            break;
        }
        if (nread == 0)
            break;
        nwritten = local_file.write(buf, nread);
        if (nwritten != nread)
        {
            SIHD_LOG_ERROR("Sftp: failed writing local file: '{}' '{} != '{}'", local_path, nwritten, nread);
            ret = false;
            break;
        }
    }
    return ret;
}

bool Sftp::mkdir(std::string_view path, mode_t mode)
{
    int r = sftp_mkdir(_sftp_session_ptr, path.data(), mode);
    if (r != 0)
        SIHD_LOG(error, "Sftp: failed to mkdir: '{}' {}", path, this->error());
    return r == SSH_FX_OK;
}

bool Sftp::symlink(std::string_view from, std::string_view to)
{
    int r = sftp_symlink(_sftp_session_ptr, from.data(), to.data());
    if (r != 0)
        SIHD_LOG_ERROR("Sftp: failed to create symbolic link from '{}' to '{}' {}", from, to, this->error());
    return r == SSH_FX_OK;
}

bool Sftp::list_dir_filenames(std::string_view path, std::vector<std::string> & list)
{
    std::vector<SftpAttribute> attrs;
    if (this->list_dir(path, attrs) == false)
        return false;
    for (const SftpAttribute & attr : attrs)
    {
        if (attr.is_dir())
        {
            std::string name = attr.name();
            name += "/";
            list.push_back(std::move(name));
        }
        else
            list.push_back(attr.name());
    }
    return true;
}

bool Sftp::list_dir(std::string_view path, std::vector<SftpAttribute> & list)
{
    SftpDir dir(sftp_opendir(_sftp_session_ptr, path.data()));
    if (dir.get() == nullptr)
    {
        SIHD_LOG(error, "Stfp: failed to open directory: {}", path);
        return false;
    }
    while (true)
    {
        SftpAttribute attribute(sftp_readdir(_sftp_session_ptr, dir.get()));
        if (attribute.attr == nullptr)
            break;
        if (strcmp(attribute.name(), ".") == 0 || strcmp(attribute.name(), "..") == 0)
            continue;
        list.push_back(std::move(attribute));
    }
    bool ret = sftp_dir_eof(dir.get()) == 1;
    if (ret == false)
        SIHD_LOG(error, "Sftp: cannot list directory: {}", path);
    return ret;
}

bool Sftp::rename(std::string_view from, std::string_view to)
{
    int r = sftp_rename(_sftp_session_ptr, from.data(), to.data());
    if (r != 0)
        SIHD_LOG_ERROR("Sftp: failed to rename '{}' to '{}' {}", from, to, this->error());
    return r == SSH_FX_OK;
}

bool Sftp::rm(std::string_view path)
{
    int r = sftp_unlink(_sftp_session_ptr, path.data());
    if (r != 0)
        SIHD_LOG(error, "Sftp: failed to remove '{}': {}", path, this->error());
    return r == SSH_FX_OK;
}

bool Sftp::rmdir(std::string_view path)
{
    int r = sftp_rmdir(_sftp_session_ptr, path.data());
    if (r != 0)
        SIHD_LOG(error, "Sftp: failed to remove directory '{}': {}", path, this->error());
    return r == SSH_FX_OK;
}

bool Sftp::chmod(std::string_view path, mode_t mode)
{
    int r = sftp_chmod(_sftp_session_ptr, path.data(), mode);
    if (r != 0)
        SIHD_LOG(error, "Sftp: failed to change permission '{}' ({}):  {}", path, mode, this->error());
    return r == SSH_FX_OK;
}

bool Sftp::chown(std::string_view path, uid_t owner, gid_t group)
{
    int r = sftp_chown(_sftp_session_ptr, path.data(), owner, group);
    if (r != 0)
        SIHD_LOG(error, "Sftp: failed to change group '{}' ({}:{}):  {}", path, owner, group, this->error());
    return r == SSH_FX_OK;
}

int Sftp::version()
{
    return sftp_server_version(_sftp_session_ptr);
}

const char *Sftp::error()
{
    switch (sftp_get_error(_sftp_session_ptr))
    {
        case SSH_FX_OK:
            return "ok";
        case SSH_FX_EOF:
            return "end of file";
        case SSH_FX_NO_SUCH_FILE:
            return "no such file";
        case SSH_FX_PERMISSION_DENIED:
            return "permission denied";
        case SSH_FX_FAILURE:
            return "failure";
        case SSH_FX_BAD_MESSAGE:
            return "bad message";
        case SSH_FX_NO_CONNECTION:
            return "no connection";
        case SSH_FX_CONNECTION_LOST:
            return "connection lost";
        case SSH_FX_OP_UNSUPPORTED:
            return "operation not supported by the server";
        case SSH_FX_INVALID_HANDLE:
            return "invalid file handle";
        case SSH_FX_NO_SUCH_PATH:
            return "no such file or directory";
        case SSH_FX_FILE_ALREADY_EXISTS:
            return "file already exists";
        case SSH_FX_WRITE_PROTECT:
            return "trying to write on a write-protected filesystem";
        case SSH_FX_NO_MEDIA:
            return "no media in remote drive";
        default:
            return "unknown";
    }
}

SftpAttribute::SftpAttribute(sftp_attributes_struct *ptr): attr(ptr) {}

SftpAttribute::SftpAttribute(SftpAttribute && sftp_attr)
{
    this->attr = sftp_attr.attr;
    sftp_attr.attr = nullptr;
}

SftpAttribute::~SftpAttribute()
{
    if (attr != nullptr)
        sftp_attributes_free(attr);
}

const char *SftpAttribute::name() const
{
    return this->attr->name;
}

uint64_t SftpAttribute::size() const
{
    return this->attr->size;
}

bool SftpAttribute::is_dir() const
{
    return this->attr->type == SSH_FILEXFER_TYPE_DIRECTORY;
}

bool SftpAttribute::is_file() const
{
    return this->attr->type == SSH_FILEXFER_TYPE_REGULAR;
}

bool SftpAttribute::is_link() const
{
    return this->attr->type == SSH_FILEXFER_TYPE_SYMLINK;
}

} // namespace sihd::ssh
