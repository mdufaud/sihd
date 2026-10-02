#include <cstring>
#include <optional>

#include <libssh/callbacks.h>

#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/utils.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/fmt.hpp>
#include <sihd/util/time.hpp>

#include "ssh_error.hpp"

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::ssh
{

SIHD_LOGGER;

struct SshCommand::Impl
{
        struct CommandStatus
        {
                int exit_status;
                bool core_dumped;
                std::string signal_str;
                std::optional<Error> exit_error;
        };

        ssh_session_struct *ssh_session_ptr;
        std::unique_ptr<struct ssh_channel_callbacks_struct> ssh_callbacks_ptr;
        SshChannel channel;
        sihd::util::Waitable waitable;
        CommandStatus command_status;
        SshCommand *parent;
        bool stop;

        Impl(ssh_session_struct *session, SshCommand *parent_cmd):
            ssh_session_ptr(session),
            parent(parent_cmd),
            stop(false)
        {
            SIHD_UNEXPECTED_LOG(utils::init());
            reset_command_status();
            ssh_callbacks_ptr = std::make_unique<struct ssh_channel_callbacks_struct>();
        }

        ~Impl() { SIHD_UNEXPECTED_LOG(utils::finalize()); }

        void reset_command_status()
        {
            command_status.exit_status = -1;
            command_status.core_dumped = false;
            command_status.signal_str.clear();
            command_status.exit_error.reset();
        }

        void callback_channel_output(char *buf, size_t size, bool is_stderr)
        {
            if (buf == nullptr || parent->output_handler == nullptr)
                return;
            parent->output_handler->handle(std::string_view {buf, size}, is_stderr);
        }

        void callback_exit_status(int exit_status)
        {
            {
                auto l = waitable.guard();
                command_status.exit_status = exit_status;
            }
            waitable.notify_all();
        }

        void callback_exit_signal(const char *signal, int core, const char *errmsg)
        {
            {
                auto l = waitable.guard();
                command_status.signal_str = signal;
                command_status.core_dumped = core != 0;
                command_status.exit_error = Error(unknown,
                                                  "command terminated by signal '{}': {}",
                                                  signal,
                                                  errmsg != nullptr ? errmsg : "");
            }
            waitable.notify_all();
        }

        static int ssh_command_channel_data_callback(ssh_session_struct *session,
                                                     ssh_channel_struct *channel,
                                                     void *data,
                                                     uint32_t len,
                                                     int is_stderr,
                                                     void *userdata)
        {
            (void)session;
            (void)channel;
            Impl *impl = static_cast<Impl *>(userdata);
            impl->callback_channel_output(reinterpret_cast<char *>(data), len, (bool)is_stderr);
            return (int)len;
        }

        static void ssh_command_exit_signal_callback(ssh_session_struct *session,
                                                     ssh_channel_struct *channel,
                                                     const char *signal,
                                                     int core,
                                                     const char *errmsg,
                                                     const char *lang,
                                                     void *userdata)
        {
            (void)session;
            (void)channel;
            (void)lang;
            Impl *impl = static_cast<Impl *>(userdata);
            impl->callback_exit_signal(signal, core, errmsg);
        }

        static void ssh_command_exit_status_callback(ssh_session_struct *session,
                                                     ssh_channel_struct *channel,
                                                     int exit_status,
                                                     void *userdata)
        {
            (void)session;
            (void)channel;
            Impl *impl = static_cast<Impl *>(userdata);
            impl->callback_exit_status(exit_status);
        }
};

SshCommand::SshCommand(void *session):
    output_handler(nullptr),
    _impl(std::make_unique<Impl>(static_cast<ssh_session_struct *>(session), this))
{
}

SshCommand::~SshCommand()
{
    {
        auto l = _impl->waitable.guard();
        _impl->stop = true;
    }
    _impl->waitable.notify_all();
}

SshChannel & SshCommand::channel()
{
    return _impl->channel;
}

std::expected<void, Error> SshCommand::execute(std::string_view cmd)
{
    if (auto res = this->execute_async(cmd); !res)
        return res;
    return this->wait();
}

std::expected<void, Error> SshCommand::execute_async(std::string_view cmd)
{
    ssh_channel channel_ptr = ssh_channel_new(_impl->ssh_session_ptr);
    if (channel_ptr == nullptr)
        return std::unexpected(
            Error(_impl->ssh_session_ptr, "could not create a ssh channel: {}", ssh_error_str(_impl->ssh_session_ptr)));
    _impl->channel.set_channel(channel_ptr);
    _impl->channel.set_blocking(false);
    if (auto res = _impl->channel.open_session(); !res)
    {
        _impl->channel.clear_channel();
        return res;
    }
    memset(_impl->ssh_callbacks_ptr.get(), 0, sizeof(struct ssh_channel_callbacks_struct));
    _impl->ssh_callbacks_ptr->userdata = (void *)_impl.get();
    _impl->ssh_callbacks_ptr->channel_data_function = Impl::ssh_command_channel_data_callback;
    _impl->ssh_callbacks_ptr->channel_exit_signal_function = Impl::ssh_command_exit_signal_callback;
    _impl->ssh_callbacks_ptr->channel_exit_status_function = Impl::ssh_command_exit_status_callback;
    ssh_callbacks_init(_impl->ssh_callbacks_ptr.get());
    if (ssh_set_channel_callbacks(static_cast<ssh_channel>(_impl->channel.channel()), _impl->ssh_callbacks_ptr.get())
        != SSH_OK)
    {
        _impl->channel.clear_channel();
        return std::unexpected(Error(_impl->ssh_session_ptr,
                                     "could not set callbacks to the channel: {}",
                                     ssh_error_str(_impl->ssh_session_ptr)));
    }
    _impl->reset_command_status();
    if (auto res = _impl->channel.request_exec(cmd); !res)
    {
        _impl->channel.clear_channel();
        return res;
    }
    _impl->channel.exit_status();
    return {};
}

std::expected<void, Error> SshCommand::wait(sihd::util::Duration timeout_nano,
                                            sihd::util::time::UnixTime milliseconds_poll_time)
{
    if (_impl->channel.is_open() == false)
        return {};
    int r = 0;
    while (_impl->stop == false && (r = _impl->channel.exit_status()) == -1)
    {
        if (timeout_nano > 0)
        {
            _impl->waitable.wait_for(timeout_nano, [this] { return _impl->stop; });
            r = _impl->channel.exit_status();
            break;
        }
        else
            _impl->waitable.wait_for(sihd::util::Duration(sihd::util::time::milliseconds(milliseconds_poll_time)),
                                     [this] { return _impl->stop; });
    }
    if (r == -1 && !_impl->command_status.exit_error)
        return std::unexpected(
            Error(_impl->stop ? interrupted : timeout, "could not wait for command: no exit status"));
    _impl->channel.clear_channel();
    // a valid exit status wins over a signal packet from a non compliant server
    if (r == -1)
        return std::unexpected(std::move(*_impl->command_status.exit_error));
    return {};
}

std::expected<void, Error> SshCommand::input(sihd::util::ArrCharView view)
{
    if (_impl->channel.is_open() == false)
        return std::unexpected(Error(closed, "could not write command input: channel is not open"));
    const int wrote = _impl->channel.write(view);
    if (wrote < 0 || static_cast<size_t>(wrote) != view.size())
        return std::unexpected(Error(io_error, "could not write command input"));
    return {};
}

int SshCommand::exit_status()
{
    return _impl->command_status.exit_status;
}

bool SshCommand::core_dumped()
{
    return _impl->command_status.core_dumped;
}

const std::string & SshCommand::exit_signal_str()
{
    return _impl->command_status.signal_str;
}

} // namespace sihd::ssh
