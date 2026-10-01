#ifndef __SIHD_SSH_SSH_ERROR_HPP__
#define __SIHD_SSH_SSH_ERROR_HPP__

#include <string>

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include <sihd/util/Error.hpp>

namespace sihd::ssh
{

std::string ssh_error_str(ssh_session_struct *session);
std::string sftp_error_str(sftp_session_struct *sftp);

} // namespace sihd::ssh

namespace sihd::util
{

template <>
struct ErrorResolver<ssh_session_struct *>
{
        static ErrorCode code(ssh_session_struct *session);
};

template <>
struct ErrorResolver<sftp_session_struct *>
{
        static ErrorCode code(sftp_session_struct *sftp);
};

} // namespace sihd::util

#endif
