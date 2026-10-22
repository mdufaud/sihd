#include <libssh/libssh.h>
#include <libssh/server.h>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/utils.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/fmt.hpp>

#include "ssh_error.hpp"

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::ssh
{

SIHD_LOGGER;

namespace
{

template <typename... Args>
Result<void> ssh_call_error(ssh_channel_struct *channel, fmt::format_string<Args...> format, Args &&...args)
{
    ssh_session session = ssh_channel_get_session(channel);
    return std::unexpected(
        Error(session, "could not {}: {}", fmt::format(format, std::forward<Args>(args)...), ssh_error_str(session)));
}

template <typename Fn, typename... Args>
Result<void> ssh_call(ssh_channel_struct *channel, Fn && fn, fmt::format_string<Args...> format, Args &&...args)
{
    if (channel == nullptr)
        return std::unexpected(
            Error(not_initialized, "could not {}: no channel", fmt::format(format, std::forward<Args>(args)...)));
    int status = 0;
    while ((status = fn()) == SSH_AGAIN)
        ;
    if (status == SSH_OK)
        return {};
    return ssh_call_error(channel, format, std::forward<Args>(args)...);
}

// teardown sends never retry SSH_AGAIN: a stalled non-blocking peer must not spin the caller
template <typename Fn, typename... Args>
Result<void> ssh_call_once(ssh_channel_struct *channel, Fn && fn, fmt::format_string<Args...> format, Args &&...args)
{
    if (channel == nullptr)
        return std::unexpected(
            Error(not_initialized, "could not {}: no channel", fmt::format(format, std::forward<Args>(args)...)));
    if (fn() == SSH_OK)
        return {};
    return ssh_call_error(channel, format, std::forward<Args>(args)...);
}

} // namespace

struct SshChannel::Impl
{
        ssh_channel_struct *ssh_channel_ptr {nullptr};
        void *userdata {nullptr};
};

SshChannel::SshChannel(void *channel): _impl_ptr(std::make_unique<Impl>())
{
    _impl_ptr->ssh_channel_ptr = static_cast<ssh_channel_struct *>(channel);
    SIHD_UNEXPECTED_LOG(utils::init());
}

SshChannel::~SshChannel()
{
    this->clear_channel();
    SIHD_UNEXPECTED_LOG(utils::finalize());
}

void SshChannel::clear_channel()
{
    if (_impl_ptr->ssh_channel_ptr != nullptr)
    {
        if (ssh_channel_is_open(_impl_ptr->ssh_channel_ptr) && !ssh_channel_is_closed(_impl_ptr->ssh_channel_ptr))
        {
            SIHD_UNEXPECTED_LOG(this->send_eof());
            SIHD_UNEXPECTED_LOG(this->close());
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

Result<void> SshChannel::open_session()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_open_session(_impl_ptr->ssh_channel_ptr); },
        "open session");
}

Result<void> SshChannel::open_agent()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_open_auth_agent(_impl_ptr->ssh_channel_ptr); },
        "open agent channel");
}

Result<void> SshChannel::open_x11(const std::string & addr, int port)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_open_x11(_impl_ptr->ssh_channel_ptr, addr.c_str(), port); },
        "open x11 channel");
}

Result<void> SshChannel::open_forward(const std::string & remotehost,
                                      int remoteport,
                                      const std::string & sourcehost,
                                      int localport)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] {
            return ssh_channel_open_forward(_impl_ptr->ssh_channel_ptr,
                                            remotehost.c_str(),
                                            remoteport,
                                            sourcehost.c_str(),
                                            localport);
        },
        "open forward channel");
}

Result<void>
    SshChannel::open_forward_unix(const std::string & remotepath, const std::string & sourcehost, int localport)
{
#if LIBSSH_VERSION_MINOR > 7
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] {
            return ssh_channel_open_forward_unix(_impl_ptr->ssh_channel_ptr,
                                                 remotepath.c_str(),
                                                 sourcehost.c_str(),
                                                 localport);
        },
        "open unix forward channel");
#else
    (void)remotepath;
    (void)sourcehost;
    (void)localport;
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return std::unexpected(Error(not_initialized, "could not open unix forward channel: no channel"));
    return std::unexpected(Error(not_supported, "could not open unix forward channel: requires libssh >= 0.8"));
#endif
}

Result<void> SshChannel::close()
{
    return ssh_call_once(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_close(_impl_ptr->ssh_channel_ptr); },
        "close channel");
}

bool SshChannel::is_open()
{
    return ssh_channel_is_open(_impl_ptr->ssh_channel_ptr) != 0;
}

Result<void> SshChannel::request_sftp()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_sftp(_impl_ptr->ssh_channel_ptr); },
        "request sftp subsystem");
}

Result<void> SshChannel::request_x11(const std::string & protocol,
                                     const std::string & cookie,
                                     int screen_number,
                                     bool single_connection)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] {
            return ssh_channel_request_x11(_impl_ptr->ssh_channel_ptr,
                                           (int)single_connection,
                                           protocol.c_str(),
                                           cookie.c_str(),
                                           screen_number);
        },
        "request x11");
}

Result<void> SshChannel::request_subsystem(const std::string & subsys)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_subsystem(_impl_ptr->ssh_channel_ptr, subsys.c_str()); },
        "request '{}' subsystem",
        subsys);
}

Result<void> SshChannel::request_pty()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_pty(_impl_ptr->ssh_channel_ptr); },
        "request pty");
}

Result<void> SshChannel::change_pty_size(int cols, int rows)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_change_pty_size(_impl_ptr->ssh_channel_ptr, cols, rows); },
        "change pty size to {}x{}",
        cols,
        rows);
}

Result<void> SshChannel::request_shell()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_shell(_impl_ptr->ssh_channel_ptr); },
        "request shell");
}

Result<void> SshChannel::request_exec(const std::string & cmd)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_exec(_impl_ptr->ssh_channel_ptr, cmd.c_str()); },
        "request exec of '{}'",
        cmd);
}

Result<void> SshChannel::set_env(const std::string & name, const std::string & value)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_env(_impl_ptr->ssh_channel_ptr, name.c_str(), value.c_str()); },
        "set env '{}'",
        name);
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

Result<void> SshChannel::cancel_forward(const std::string & addr, int port)
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return std::unexpected(Error(not_initialized, "could not cancel forward: no channel"));
    ssh_session session = ssh_channel_get_session(_impl_ptr->ssh_channel_ptr);
    if (session == nullptr)
        return std::unexpected(Error(not_initialized, "could not cancel forward: no session"));
    if (ssh_channel_cancel_forward(session, addr.c_str(), port) == SSH_OK)
        return {};
    return std::unexpected(
        Error(session, "could not cancel forward of '{}:{}': {}", addr, port, ssh_error_str(session)));
}

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

Result<void> SshChannel::send_signal(const std::string & sig)
{
    return ssh_call_once(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_send_signal(_impl_ptr->ssh_channel_ptr, sig.c_str()); },
        "send signal '{}'",
        sig);
}

Result<void> SshChannel::send_eof()
{
    return ssh_call_once(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_send_eof(_impl_ptr->ssh_channel_ptr); },
        "send eof");
}

bool SshChannel::is_eof()
{
    return ssh_channel_is_eof(_impl_ptr->ssh_channel_ptr) != 0;
}

Result<void> SshChannel::request_send_exit_status(int exit_status)
{
    return ssh_call_once(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_send_exit_status(_impl_ptr->ssh_channel_ptr, exit_status); },
        "send exit status");
}

Result<void> SshChannel::request_send_exit_signal(const std::string & signum,
                                                  bool core_dumped,
                                                  const std::string & errmsg,
                                                  const std::string & lang)
{
    return ssh_call_once(
        _impl_ptr->ssh_channel_ptr,
        [&] {
            return ssh_channel_request_send_exit_signal(_impl_ptr->ssh_channel_ptr,
                                                        signum.c_str(),
                                                        core_dumped ? 1 : 0,
                                                        errmsg.c_str(),
                                                        lang.c_str());
        },
        "send exit signal '{}'",
        signum);
}

int SshChannel::read(IArray & array)
{
    int ret = ssh_channel_read(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 0);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_stderr(IArray & array)
{
    int ret = ssh_channel_read(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 1);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_timeout(IArray & array, int timeout_ms)
{
    int ret = ssh_channel_read_timeout(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 0, timeout_ms);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_timeout_stderr(IArray & array, int timeout_ms)
{
    int ret = ssh_channel_read_timeout(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 1, timeout_ms);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_nonblock(IArray & array)
{
    int ret = ssh_channel_read_nonblocking(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 0);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_nonblock_stderr(IArray & array)
{
    int ret = ssh_channel_read_nonblocking(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 1);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::write(ArrCharView view)
{
    return ssh_channel_write(_impl_ptr->ssh_channel_ptr, view.data(), view.size());
}

int SshChannel::write_stderr(ArrCharView view)
{
    return ssh_channel_write_stderr(_impl_ptr->ssh_channel_ptr, view.data(), view.size());
}

} // namespace sihd::ssh
