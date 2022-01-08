#include <sihd/ssh/Sftp.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::ssh
{

LOGGER;

Sftp::Sftp(ssh_session session): _ssh_session_ptr(session), _sftp_session_ptr(nullptr)
{
}

Sftp::~Sftp()
{
    this->close();
}

bool    Sftp::open()
{
    _sftp_session_ptr = sftp_new(_ssh_session_ptr);
    if (_sftp_session_ptr == nullptr)
    {
        LOG(error, "Sftp: failed to create sftp session: " << ssh_get_error(_ssh_session_ptr));
        return false;
    }
    int r = sftp_init(_sftp_session_ptr);
    if (r != SSH_FX_OK)
    {
        LOG(error, "Sftp: failed to initialize sftp session: " << this->error());
        this->close();
        return false;
    }
    return true;
}

void    Sftp::close()
{
    if (_sftp_session_ptr != nullptr)
    {
        sftp_free(_sftp_session_ptr);
        _sftp_session_ptr = nullptr;
    }
}

bool    Sftp::mkdir(std::string_view path, mode_t mode)
{
    int r = sftp_mkdir(_sftp_session_ptr, path.data(), mode);
    if (r != 0)
        LOG(error, "Sftp: failed to mkdir: '" << path << "' " << this->error());
    return r == SSH_FX_OK;
}

bool    Sftp::rm(std::string_view path)
{
    int r = sftp_unlink(_sftp_session_ptr, path.data());
    if (r != 0)
        LOG(error, "Sftp: failed to remove: '" << path << "' " << this->error());
    return r == SSH_FX_OK;
}

int     Sftp::version()
{
    return sftp_server_version(_sftp_session_ptr);
}

const char  *Sftp::error()
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

}