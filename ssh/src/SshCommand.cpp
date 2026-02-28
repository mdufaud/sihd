#include <libssh/callbacks.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/utils.hpp>

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
                std::string err_msg;
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
            utils::init();
            reset_command_status();
            ssh_callbacks_ptr = std::make_unique<struct ssh_channel_callbacks_struct>();
        }

        ~Impl() { utils::finalize(); }

        void reset_command_status()
        {
            command_status.exit_status = -1;
            command_status.core_dumped = false;
            command_status.err_msg.clear();
            command_status.signal_str.clear();
        }

        void callback_channel_output(char *buf, size_t size, bool is_stderr)
        {
            if (buf == nullptr || parent->output_handler == nullptr)
                return;
            parent->output_handler->handle(std::string_view {buf, size}, is_stderr);
        }

        void callback_exit_status(int exit_status)
        {
            auto l = waitable.guard();
            command_status.exit_status = exit_status;
            waitable.notify_all();
        }

        void callback_exit_signal(const char *signal, int core, const char *errmsg)
        {
            auto l = waitable.guard();
            command_status.signal_str = signal;
            command_status.err_msg = errmsg;
            command_status.core_dumped = core != 0;
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

bool SshCommand::execute(std::string_view cmd)
{
    return this->execute_async(cmd) && this->wait();
}

bool SshCommand::execute_async(std::string_view cmd)
{
    ssh_channel channel_ptr = ssh_channel_new(_impl->ssh_session_ptr);
    if (channel_ptr == nullptr)
    {
        SIHD_LOG(error,
                 "SshCommand: failed to create a ssh channel: {}",
                 ssh_get_error(_impl->ssh_session_ptr));
        return false;
    }
    _impl->channel.set_channel(channel_ptr);
    _impl->channel.set_blocking(false);
    if (_impl->channel.open_session() == false)
    {
        SIHD_LOG(error, "SshCommand: failed to open a ssh channel session");
        _impl->channel.clear_channel();
        return false;
    }
    memset(_impl->ssh_callbacks_ptr.get(), 0, sizeof(struct ssh_channel_callbacks_struct));
    _impl->ssh_callbacks_ptr->userdata = (void *)_impl.get();
    _impl->ssh_callbacks_ptr->channel_data_function = Impl::ssh_command_channel_data_callback;
    _impl->ssh_callbacks_ptr->channel_exit_signal_function = Impl::ssh_command_exit_signal_callback;
    _impl->ssh_callbacks_ptr->channel_exit_status_function = Impl::ssh_command_exit_status_callback;
    ssh_callbacks_init(_impl->ssh_callbacks_ptr.get());
    if (ssh_set_channel_callbacks(static_cast<ssh_channel>(_impl->channel.channel()), _impl->ssh_callbacks_ptr.get()) != SSH_OK)
    {
        SIHD_LOG(error, "SshCommand: failed to set callbacks to the channel");
        _impl->channel.clear_channel();
        return false;
    }
    _impl->reset_command_status();
    bool ret = _impl->channel.request_exec(cmd);
    if (!ret)
    {
        SIHD_LOG(error, "SshCommand: failed to execute command: {}", cmd);
        _impl->channel.clear_channel();
    }
    else
        _impl->channel.exit_status();
    return ret;
}

bool SshCommand::wait(time_t timeout_nano, time_t milliseconds_poll_time)
{
    if (_impl->channel.is_open() == false)
        return true;
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
            _impl->waitable.wait_for(sihd::util::time::milliseconds(milliseconds_poll_time),
                                     [this] { return _impl->stop; });
    }
    if (r != -1)
        _impl->channel.clear_channel();
    return r != -1;
}

bool SshCommand::input(sihd::util::ArrCharView view)
{
    return _impl->channel.is_open() && _impl->channel.write(view);
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

const std::string & SshCommand::exit_signal_error()
{
    return _impl->command_status.err_msg;
}

} // namespace sihd::ssh
