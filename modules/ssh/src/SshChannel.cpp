#include <libssh/libssh.h>
#include <libssh/server.h>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/utils.hpp>

#include <sihd/util/Logger.hpp>

#define WHILE_SSH_AGAIN_AND_RETURN(method)                                                                   \
    {                                                                                                        \
        int r;                                                                                               \
        while ((r = method) == SSH_AGAIN)                                                                    \
        {                                                                                                    \
            ;                                                                                                \
        }                                                                                                    \
        return r == SSH_OK;                                                                                  \
    }

namespace sihd::ssh
{

SIHD_LOGGER;

struct SshChannel::Impl
{
        ssh_channel_struct *ssh_channel_ptr {nullptr};
        void *userdata {nullptr};
};

SshChannel::SshChannel(void *channel): _impl_ptr(std::make_unique<Impl>())
{
    _impl_ptr->ssh_channel_ptr = static_cast<ssh_channel_struct *>(channel);
    utils::init();
}

SshChannel::~SshChannel()
{
    this->clear_channel();
    utils::finalize();
}

void SshChannel::clear_channel()
{
    if (_impl_ptr->ssh_channel_ptr != nullptr)
    {
        // Only try to close/send_eof if the channel is still open
        if (ssh_channel_is_open(_impl_ptr->ssh_channel_ptr)
            && !ssh_channel_is_closed(_impl_ptr->ssh_channel_ptr))
        {
            this->send_eof();
            this->close();
        }
        ssh_channel_free(_impl_ptr->ssh_channel_ptr);
        _impl_ptr->ssh_channel_ptr = nullptr;
    }
}

void SshChannel::set_channel(void *channel)
{
    this->clear_channel();
    _impl_ptr->ssh_channel_ptr = static_cast<ssh_channel_struct *>(channel);
}

bool SshChannel::open_session()
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return false;
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_open_session(_impl_ptr->ssh_channel_ptr));
}

bool SshChannel::open_agent()
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return false;
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_open_auth_agent(_impl_ptr->ssh_channel_ptr));
}

bool SshChannel::open_x11(std::string_view addr, int port)
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return false;
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_open_x11(_impl_ptr->ssh_channel_ptr, addr.data(), port));
}

bool SshChannel::open_forward(std::string_view remotehost,
                              int remoteport,
                              std::string_view sourcehost,
                              int localport)
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return false;
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_open_forward(_impl_ptr->ssh_channel_ptr,
                                                        remotehost.data(),
                                                        remoteport,
                                                        sourcehost.data(),
                                                        localport));
}

bool SshChannel::open_forward_unix(std::string_view remotepath, std::string_view sourcehost, int localport)
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return false;
#if LIBSSH_VERSION_MINOR > 7
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_open_forward_unix(
        _impl_ptr->ssh_channel_ptr, remotepath.data(), sourcehost.data(), localport));
#else
    (void)remotepath;
    (void)sourcehost;
    (void)localport;
    SIHD_LOG(error, "SshChannel: open_forward_unix requires libssh >= 0.8");
    return false;
#endif
}

bool SshChannel::close()
{
    return ssh_channel_close(_impl_ptr->ssh_channel_ptr) == SSH_OK;
}

bool SshChannel::is_open()
{
    return ssh_channel_is_open(_impl_ptr->ssh_channel_ptr) != 0;
}

bool SshChannel::request_sftp()
{
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_request_sftp(_impl_ptr->ssh_channel_ptr));
}

bool SshChannel::request_x11(std::string_view protocol,
                             std::string_view cookie,
                             int screen_number,
                             bool single_connection)
{
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_request_x11(_impl_ptr->ssh_channel_ptr,
                                                       (int)single_connection,
                                                       protocol.data(),
                                                       cookie.data(),
                                                       screen_number));
}

bool SshChannel::request_subsystem(std::string_view subsys)
{
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_request_subsystem(_impl_ptr->ssh_channel_ptr, subsys.data()));
}

bool SshChannel::request_pty()
{
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_request_pty(_impl_ptr->ssh_channel_ptr));
}

bool SshChannel::change_pty_size(int cols, int rows)
{
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_change_pty_size(_impl_ptr->ssh_channel_ptr, cols, rows));
}

bool SshChannel::request_shell()
{
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_request_shell(_impl_ptr->ssh_channel_ptr));
}

bool SshChannel::request_exec(std::string_view cmd)
{
    WHILE_SSH_AGAIN_AND_RETURN(ssh_channel_request_exec(_impl_ptr->ssh_channel_ptr, cmd.data()));
}

/* ************************************************************************* */
/* utils */
/* ************************************************************************* */

bool SshChannel::set_env(std::string_view name, std::string_view value)
{
    WHILE_SSH_AGAIN_AND_RETURN(
        ssh_channel_request_env(_impl_ptr->ssh_channel_ptr, name.data(), value.data()));
}

int SshChannel::exit_status()
{
    return ssh_channel_get_exit_status(_impl_ptr->ssh_channel_ptr);
}

void SshChannel::set_blocking(bool active)
{
    ssh_channel_set_blocking(_impl_ptr->ssh_channel_ptr, (int)active);
}

void *SshChannel::session() const
{
    return ssh_channel_get_session(_impl_ptr->ssh_channel_ptr);
}

void *SshChannel::channel() const
{
    return _impl_ptr->ssh_channel_ptr;
}

void SshChannel::set_userdata(void *userdata)
{
    _impl_ptr->userdata = userdata;
}

void *SshChannel::userdata() const
{
    return _impl_ptr->userdata;
}

void SshChannel::detach()
{
    _impl_ptr->ssh_channel_ptr = nullptr;
}

bool SshChannel::cancel_forward(std::string_view addr, int port)
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return false;
    ssh_session session = ssh_channel_get_session(_impl_ptr->ssh_channel_ptr);
    if (session == nullptr)
        return false;
    return ssh_channel_cancel_forward(session, addr.data(), port) == SSH_OK;
}

/* ************************************************************************* */
/* poll */
/* ************************************************************************* */

int SshChannel::poll()
{
    return ssh_channel_poll(_impl_ptr->ssh_channel_ptr, 0);
}

int SshChannel::poll_stderr()
{
    return ssh_channel_poll(_impl_ptr->ssh_channel_ptr, 1);
}

int SshChannel::poll_timeout(int timeout_ms)
{
    return ssh_channel_poll_timeout(_impl_ptr->ssh_channel_ptr, timeout_ms, 0);
}

int SshChannel::poll_timeout_stderr(int timeout_ms)
{
    return ssh_channel_poll_timeout(_impl_ptr->ssh_channel_ptr, timeout_ms, 1);
}

/* ************************************************************************* */
/* read - write */
/* ************************************************************************* */

bool SshChannel::send_signal(std::string_view sig)
{
    return ssh_channel_request_send_signal(_impl_ptr->ssh_channel_ptr, sig.data());
}

bool SshChannel::send_eof()
{
    return ssh_channel_send_eof(_impl_ptr->ssh_channel_ptr) == SSH_OK;
}

bool SshChannel::is_eof()
{
    return ssh_channel_is_eof(_impl_ptr->ssh_channel_ptr) != 0;
}

bool SshChannel::request_send_exit_status(int exit_status)
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return false;
    return ssh_channel_request_send_exit_status(_impl_ptr->ssh_channel_ptr, exit_status) == SSH_OK;
}

bool SshChannel::request_send_exit_signal(std::string_view signum,
                                          bool core_dumped,
                                          std::string_view errmsg,
                                          std::string_view lang)
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return false;
    return ssh_channel_request_send_exit_signal(_impl_ptr->ssh_channel_ptr,
                                                signum.data(),
                                                core_dumped ? 1 : 0,
                                                errmsg.data(),
                                                lang.data())
           == SSH_OK;
}

int SshChannel::read(sihd::util::IArray & array)
{
    int ret = ssh_channel_read(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 0);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_stderr(sihd::util::IArray & array)
{
    int ret = ssh_channel_read(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 1);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_timeout(sihd::util::IArray & array, int timeout_ms)
{
    int ret = ssh_channel_read_timeout(
        _impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 0, timeout_ms);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_timeout_stderr(sihd::util::IArray & array, int timeout_ms)
{
    int ret = ssh_channel_read_timeout(
        _impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 1, timeout_ms);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_nonblock(sihd::util::IArray & array)
{
    int ret = ssh_channel_read_nonblocking(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 0);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_nonblock_stderr(sihd::util::IArray & array)
{
    int ret = ssh_channel_read_nonblocking(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 1);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::write(sihd::util::ArrCharView view)
{
    return ssh_channel_write(_impl_ptr->ssh_channel_ptr, view.data(), view.size());
}

int SshChannel::write_stderr(sihd::util::ArrCharView view)
{
    return ssh_channel_write_stderr(_impl_ptr->ssh_channel_ptr, view.data(), view.size());
}

} // namespace sihd::ssh