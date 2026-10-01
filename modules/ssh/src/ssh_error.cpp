#include "ssh_error.hpp"

#include <sihd/util/fmt.hpp>

namespace sihd::ssh
{

namespace
{

const char *sftp_error_description(int code)
{
    switch (code)
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

} // namespace

} // namespace sihd::ssh

namespace sihd::util
{

sihd::util::ErrorCode ErrorResolver<ssh_session_struct *>::code(ssh_session_struct *session)
{
    if (session == nullptr)
        return ErrorCode::not_initialized;
    switch (ssh_get_error_code(session))
    {
        case SSH_NO_ERROR:
            return ErrorCode::unknown;
        case SSH_REQUEST_DENIED:
            return ErrorCode::permission_denied;
        case SSH_EINTR:
            return ErrorCode::interrupted;
        case SSH_AGAIN:
            return ErrorCode::would_block;
        case SSH_ERROR:
            return ErrorCode::io_error;
        default:
            return ErrorCode::unknown;
    }
}

sihd::util::ErrorCode ErrorResolver<sftp_session_struct *>::code(sftp_session_struct *sftp)
{
    if (sftp == nullptr)
        return ErrorCode::not_initialized;
    switch (sftp_get_error(sftp))
    {
        case SSH_FX_OK:
            return ErrorCode::unknown;
        case SSH_FX_NO_SUCH_FILE:
        case SSH_FX_NO_SUCH_PATH:
        case SSH_FX_INVALID_HANDLE:
            return ErrorCode::not_found;
        case SSH_FX_PERMISSION_DENIED:
        case SSH_FX_WRITE_PROTECT:
            return ErrorCode::permission_denied;
        case SSH_FX_FILE_ALREADY_EXISTS:
            return ErrorCode::already_exists;
        case SSH_FX_OP_UNSUPPORTED:
            return ErrorCode::not_supported;
        case SSH_FX_NO_CONNECTION:
        case SSH_FX_CONNECTION_LOST:
            return ErrorCode::closed;
        case SSH_FX_BAD_MESSAGE:
            return ErrorCode::invalid_argument;
        default:
            return ErrorCode::io_error;
    }
}

} // namespace sihd::util

namespace sihd::ssh
{

std::string ssh_error_str(ssh_session_struct *session)
{
    if (session == nullptr)
        return "no session";
    const int code = ssh_get_error_code(session);
    return fmt::format("{} (code = {})", ssh_get_error(session), code);
}

std::string sftp_error_str(sftp_session_struct *sftp)
{
    if (sftp == nullptr)
        return "no sftp session";
    const int code = sftp_get_error(sftp);
    return fmt::format("{} (code = {})", sftp_error_description(code), code);
}

} // namespace sihd::ssh
