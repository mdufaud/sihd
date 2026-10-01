#include <libssh/libssh.h>
#include <libssh/server.h>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/utils.hpp>
#include <sihd/util/Logger.hpp>
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
std::expected<void, Error>
    ssh_call_error(ssh_channel_struct *channel, fmt::format_string<Args...> format, Args &&...args)
{
    ssh_session session = ssh_channel_get_session(channel);
    return std::unexpected(
        Error(session, "could not {}: {}", fmt::format(format, std::forward<Args>(args)...), ssh_error_str(session)));
}

template <typename Fn, typename... Args>
std::expected<void, Error>
    ssh_call(ssh_channel_struct *channel, Fn && fn, fmt::format_string<Args...> format, Args &&...args)
{
    if (channel == nullptr)
        return std::unexpected(
            Error(not_initialized, "could not {}: no channel", fmt::format(format, std::forward<Args>(args)...)));
    int r = 0;
    while ((r = fn()) == SSH_AGAIN)
        ;
    if (r == SSH_OK)
        return {};
    return ssh_call_error(channel, format, std::forward<Args>(args)...);
}

// teardown sends never retry SSH_AGAIN: a stalled non-blocking peer must not spin the caller
template <typename Fn, typename... Args>
std::expected<void, Error>
    ssh_call_once(ssh_channel_struct *channel, Fn && fn, fmt::format_string<Args...> format, Args &&...args)
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
        // Only try to close/send_eof if the channel is still open
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

std::expected<void, Error> SshChannel::open_session()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_open_session(_impl_ptr->ssh_channel_ptr); },
        "open session");
}

std::expected<void, Error> SshChannel::open_agent()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_open_auth_agent(_impl_ptr->ssh_channel_ptr); },
        "open agent channel");
}

std::expected<void, Error> SshChannel::open_x11(std::string_view addr, int port)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_open_x11(_impl_ptr->ssh_channel_ptr, addr.data(), port); },
        "open x11 channel");
}

std::expected<void, Error>
    SshChannel::open_forward(std::string_view remotehost, int remoteport, std::string_view sourcehost, int localport)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] {
            return ssh_channel_open_forward(_impl_ptr->ssh_channel_ptr,
                                            remotehost.data(),
                                            remoteport,
                                            sourcehost.data(),
                                            localport);
        },
        "open forward channel");
}

std::expected<void, Error>
    SshChannel::open_forward_unix(std::string_view remotepath, std::string_view sourcehost, int localport)
{
#if LIBSSH_VERSION_MINOR > 7
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] {
            return ssh_channel_open_forward_unix(_impl_ptr->ssh_channel_ptr,
                                                 remotepath.data(),
                                                 sourcehost.data(),
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

std::expected<void, Error> SshChannel::close()
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

std::expected<void, Error> SshChannel::request_sftp()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_sftp(_impl_ptr->ssh_channel_ptr); },
        "request sftp subsystem");
}

std::expected<void, Error> SshChannel::request_x11(std::string_view protocol,
                                                   std::string_view cookie,
                                                   int screen_number,
                                                   bool single_connection)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] {
            return ssh_channel_request_x11(_impl_ptr->ssh_channel_ptr,
                                           (int)single_connection,
                                           protocol.data(),
                                           cookie.data(),
                                           screen_number);
        },
        "request x11");
}

std::expected<void, Error> SshChannel::request_subsystem(std::string_view subsys)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_subsystem(_impl_ptr->ssh_channel_ptr, subsys.data()); },
        "request '{}' subsystem",
        subsys);
}

std::expected<void, Error> SshChannel::request_pty()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_pty(_impl_ptr->ssh_channel_ptr); },
        "request pty");
}

std::expected<void, Error> SshChannel::change_pty_size(int cols, int rows)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_change_pty_size(_impl_ptr->ssh_channel_ptr, cols, rows); },
        "change pty size to {}x{}",
        cols,
        rows);
}

std::expected<void, Error> SshChannel::request_shell()
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_shell(_impl_ptr->ssh_channel_ptr); },
        "request shell");
}

std::expected<void, Error> SshChannel::request_exec(std::string_view cmd)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_exec(_impl_ptr->ssh_channel_ptr, cmd.data()); },
        "request exec of '{}'",
        cmd);
}

std::expected<void, Error> SshChannel::set_env(std::string_view name, std::string_view value)
{
    return ssh_call(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_env(_impl_ptr->ssh_channel_ptr, name.data(), value.data()); },
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

std::expected<void, Error> SshChannel::cancel_forward(std::string_view addr, int port)
{
    if (_impl_ptr->ssh_channel_ptr == nullptr)
        return std::unexpected(Error(not_initialized, "could not cancel forward: no channel"));
    ssh_session session = ssh_channel_get_session(_impl_ptr->ssh_channel_ptr);
    if (session == nullptr)
        return std::unexpected(Error(not_initialized, "could not cancel forward: no session"));
    if (ssh_channel_cancel_forward(session, addr.data(), port) == SSH_OK)
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

std::expected<void, Error> SshChannel::send_signal(std::string_view sig)
{
    return ssh_call_once(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_send_signal(_impl_ptr->ssh_channel_ptr, sig.data()); },
        "send signal '{}'",
        sig);
}

std::expected<void, Error> SshChannel::send_eof()
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

std::expected<void, Error> SshChannel::request_send_exit_status(int exit_status)
{
    return ssh_call_once(
        _impl_ptr->ssh_channel_ptr,
        [&] { return ssh_channel_request_send_exit_status(_impl_ptr->ssh_channel_ptr, exit_status); },
        "send exit status");
}

std::expected<void, Error> SshChannel::request_send_exit_signal(std::string_view signum,
                                                                bool core_dumped,
                                                                std::string_view errmsg,
                                                                std::string_view lang)
{
    return ssh_call_once(
        _impl_ptr->ssh_channel_ptr,
        [&] {
            return ssh_channel_request_send_exit_signal(_impl_ptr->ssh_channel_ptr,
                                                        signum.data(),
                                                        core_dumped ? 1 : 0,
                                                        errmsg.data(),
                                                        lang.data());
        },
        "send exit signal '{}'",
        signum);
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
    int ret = ssh_channel_read_timeout(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 0, timeout_ms);
    array.byte_resize(std::max(0, ret));
    return ret;
}

int SshChannel::read_timeout_stderr(sihd::util::IArray & array, int timeout_ms)
{
    int ret = ssh_channel_read_timeout(_impl_ptr->ssh_channel_ptr, array.buf(), array.byte_capacity(), 1, timeout_ms);
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
