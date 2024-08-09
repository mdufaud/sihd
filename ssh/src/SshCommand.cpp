#include <libssh/callbacks.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include <sihd/ssh/SshCommand.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

int SshCommand::ssh_command_channel_data_callback(ssh_session_struct *session,
                                                  ssh_channel_struct *channel,
                                                  void *data,
                                                  uint32_t len,
                                                  int is_stderr,
                                                  void *userdata)
{
    (void)session;
    (void)channel;
    SshCommand *sshcommand = (SshCommand *)userdata;
    sshcommand->_callback_channel_output(reinterpret_cast<char *>(data), len, (bool)is_stderr);
    return (int)len;
}

void SshCommand::ssh_command_exit_signal_callback(ssh_session_struct *session,
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
    SshCommand *sshcommand = (SshCommand *)userdata;
    sshcommand->_callback_exit_signal(signal, core, errmsg);
}

void SshCommand::ssh_command_exit_status_callback(ssh_session_struct *session,
                                                  ssh_channel_struct *channel,
                                                  int exit_status,
                                                  void *userdata)
{
    (void)session;
    (void)channel;
    SshCommand *sshcommand = (SshCommand *)userdata;
    sshcommand->_callback_exit_status(exit_status);
}

SshCommand::SshCommand(ssh_session_struct *session): output_handler(nullptr), _ssh_session_ptr(session), _stop(false)
{
    this->_reset_command_status();
    _ssh_callbacks_ptr = std::make_unique<struct ssh_channel_callbacks_struct>();
}

SshCommand::~SshCommand()
{
    auto l = _waitable.guard();
    _stop = true;
    _waitable.notify_all();
}

void SshCommand::_reset_command_status()
{
    _command_status.exit_status = -1;
    _command_status.core_dumped = false;
    _command_status.err_msg.clear();
    _command_status.signal_str.clear();
}

void SshCommand::_callback_channel_output(char *buf, size_t size, bool is_stderr)
{
    if (buf == nullptr || this->output_handler == nullptr)
        return;
    this->output_handler->handle(std::string_view {buf, size}, is_stderr);
}

void SshCommand::_callback_exit_status(int exit_status)
{
    auto l = _waitable.guard();
    _command_status.exit_status = exit_status;
    _waitable.notify_all();
}

void SshCommand::_callback_exit_signal(const char *signal, int core, const char *errmsg)
{
    auto l = _waitable.guard();
    _command_status.signal_str = signal;
    _command_status.err_msg = errmsg;
    _command_status.core_dumped = core != 0;
    _waitable.notify_all();
}

bool SshCommand::execute(std::string_view cmd)
{
    return this->_execute(cmd, false);
}

bool SshCommand::execute_async(std::string_view cmd)
{
    return this->_execute(cmd, true);
}

bool SshCommand::_execute(std::string_view cmd, bool async)
{
    ssh_channel channel_ptr = ssh_channel_new(_ssh_session_ptr);
    if (channel_ptr == nullptr)
    {
        SIHD_LOG(error, "SshCommand: failed to create a ssh channel: {}", ssh_get_error(_ssh_session_ptr));
        return false;
    }
    _channel.set_channel(channel_ptr);
    _channel.set_blocking(async == false);
    if (_channel.open_session() == false)
    {
        SIHD_LOG(error, "SshCommand: failed to open a ssh channel session");
        _channel.clear_channel();
        return false;
    }
    memset(_ssh_callbacks_ptr.get(), 0, sizeof(struct ssh_channel_callbacks_struct));
    _ssh_callbacks_ptr->userdata = (void *)this;
    _ssh_callbacks_ptr->channel_data_function = SshCommand::ssh_command_channel_data_callback;
    _ssh_callbacks_ptr->channel_exit_signal_function = SshCommand::ssh_command_exit_signal_callback;
    _ssh_callbacks_ptr->channel_exit_status_function = SshCommand::ssh_command_exit_status_callback;
    ssh_callbacks_init(_ssh_callbacks_ptr.get());
    if (ssh_set_channel_callbacks(_channel.channel(), _ssh_callbacks_ptr.get()) != SSH_OK)
    {
        SIHD_LOG(error, "SshCommand: failed to set callbacks to the channel");
        _channel.clear_channel();
        return false;
    }
    this->_reset_command_status();
    bool ret = _channel.request_exec(cmd);
    if (!ret)
    {
        SIHD_LOG(error, "SshCommand: failed to execute command: {}", cmd);
        _channel.clear_channel();
    }
    else
        _channel.exit_status();
    return ret;
}

bool SshCommand::wait(time_t timeout_nano, time_t milliseconds_poll_time)
{
    if (_channel.is_open() == false)
        return true;
    int r = 0;
    while (_stop == false && (r = _channel.exit_status()) == -1)
    {
        if (timeout_nano > 0)
        {
            _waitable.wait_for(timeout_nano, [this] { return _stop; });
            r = _channel.exit_status();
            break;
        }
        else
            _waitable.wait_for(sihd::util::time::milliseconds(milliseconds_poll_time), [this] { return _stop; });
    }
    if (r != -1)
        _channel.clear_channel();
    return r != -1;
}

bool SshCommand::input(sihd::util::ArrCharView view)
{
    return _channel.is_open() && _channel.write(view);
}

int SshCommand::exit_status()
{
    return _command_status.exit_status;
}

bool SshCommand::core_dumped()
{
    return _command_status.core_dumped;
}

const std::string & SshCommand::exit_signal_str()
{
    return _command_status.signal_str;
}

const std::string & SshCommand::exit_signal_error()
{
    return _command_status.err_msg;
}

} // namespace sihd::ssh
