#include <sihd/ssh/SshCommand.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

static int ssh_command_channel_data_callback(ssh_session session, ssh_channel channel,
                                                void *data, uint32_t len, int is_stderr,
                                                void *userdata)
{
    (void)session;
    (void)channel;
    SshCommand *sshcommand = (SshCommand *)userdata;
    sshcommand->_callback_channel_output(reinterpret_cast<char *>(data), len, (bool)is_stderr);
    return (int)len;
}

static void ssh_command_exit_signal_callback(ssh_session session, ssh_channel channel, const char *signal, int core,
                                                const char *errmsg, const char *lang, void *userdata)
{
    (void)session;
    (void)channel;
    (void)lang;
    SshCommand *sshcommand = (SshCommand *)userdata;
    sshcommand->_callback_exit_signal(signal, core, errmsg);
}

static void ssh_command_exit_status_callback(ssh_session session, ssh_channel channel, int exit_status, void *userdata)
{
    (void)session;
    (void)channel;
    SshCommand *sshcommand = (SshCommand *)userdata;
    sshcommand->_callback_exit_status(exit_status);
}

SshCommand::SshCommand(ssh_session session):
    output_handler(nullptr), _ssh_session_ptr(session), _stop(false)
{
    this->_reset_command_status();
}

SshCommand::~SshCommand()
{
    _stop = true;
}

void    SshCommand::_reset_command_status()
{
    _command_status.exit_status = -1;
    _command_status.core_dumped = false;
    _command_status.err_msg.clear();
    _command_status.signal_str.clear();
}

void    SshCommand::_callback_channel_output(char *buf, size_t size, bool is_stderr)
{
    if (this->output_handler != nullptr)
        this->output_handler->handle(buf, size, is_stderr);
}

void    SshCommand::_callback_exit_status(int exit_status)
{
    _command_status.exit_status = exit_status;
    _waitable.notify_all();
}

void    SshCommand::_callback_exit_signal(const char *signal, int core, const char *errmsg)
{
    _command_status.signal_str = signal;
    _command_status.err_msg = errmsg;
    _command_status.core_dumped = core != 0;
    _waitable.notify_all();
}

bool    SshCommand::execute(const std::string & cmd)
{
    return this->_execute(cmd, false);
}

bool    SshCommand::execute_async(const std::string & cmd)
{
    return this->_execute(cmd, true);
}

bool    SshCommand::_execute(const std::string & cmd, bool async)
{
    ssh_channel channel_ptr = ssh_channel_new(_ssh_session_ptr);
    if (channel_ptr == nullptr)
    {
        SIHD_LOG(error, "SshCommand: failed to create a ssh channel: " << ssh_get_error(_ssh_session_ptr));
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
    memset(&_ssh_callbacks, 0, sizeof(_ssh_callbacks));
    _ssh_callbacks.userdata = (void *)this;
    _ssh_callbacks.channel_data_function = ssh_command_channel_data_callback;
    _ssh_callbacks.channel_exit_signal_function = ssh_command_exit_signal_callback;
    _ssh_callbacks.channel_exit_status_function = ssh_command_exit_status_callback;
    ssh_callbacks_init(&_ssh_callbacks);
    if (ssh_set_channel_callbacks(_channel.channel(), &_ssh_callbacks) != SSH_OK)
    {
        SIHD_LOG(error, "SshCommand: failed to set callbacks to the channel");
        _channel.clear_channel();
        return false;
    }
    this->_reset_command_status();
    bool ret = _channel.request_exec(cmd);
    if (!ret)
    {
        SIHD_LOG(error, "SshCommand: failed to execute command: " << cmd);
        _channel.clear_channel();
    }
    else
        _channel.exit_status();
    return ret;
}

bool    SshCommand::wait(time_t timeout_nano)
{
    if (_channel.is_open() == false)
        return true;
    int r;
    while ((r = _channel.exit_status()) == -1 && _stop == false)
    {
        if (timeout_nano > 0)
        {
            _waitable.wait_for(timeout_nano);
            r = _channel.exit_status();
            break ;
        }
        else
            _waitable.wait_for(sihd::util::time::sec(1));
    }
    if (r != -1)
        _channel.clear_channel();
    return r != -1;
}

bool    SshCommand::input(const char *buf, size_t size)
{
    return _channel.is_open() && _channel.write(buf, size);
}

int    SshCommand::exit_status()
{
    return _command_status.exit_status;
}

bool    SshCommand::core_dumped()
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

}