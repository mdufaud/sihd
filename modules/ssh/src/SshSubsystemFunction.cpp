#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshSubsystemFunction.hpp>
#include <sihd/ssh/utils.hpp>
#include <sihd/util/Logger.hpp>

#include "channel_output.hpp"

using namespace sihd::util;

namespace sihd::ssh
{

SIHD_LOGGER;

SshSubsystemFunction::SshSubsystemFunction(std::string_view command):
    _channel(nullptr),
    _command(command),
    _exit_code(0)
{
    SIHD_UNEXPECTED_LOG(utils::init());
}

SshSubsystemFunction::~SshSubsystemFunction()
{
    SIHD_UNEXPECTED_LOG(utils::finalize());
}

void SshSubsystemFunction::set_callback(Callback callback)
{
    _callback = std::move(callback);
}

bool SshSubsystemFunction::on_start(SshChannel *channel,
                                    [[maybe_unused]] bool has_pty,
                                    [[maybe_unused]] const WinSize & winsize)
{
    _channel = channel;

    if (!_callback)
    {
        SIHD_LOG(error, "SshSubsystemFunction: no callback set for command '{}'", _command);
        return false;
    }

    SIHD_LOG(debug, "SshSubsystemFunction: exec '{}'", _command);

    Result result = _callback(_command);
    _exit_code = result.exit_code;

    // BasicSshServerHandler::on_poll sends exit status, eof and close once all of it is written
    _unsent_out = std::move(result.output);
    _unsent_err = std::move(result.error_output);
    this->forward_output();

    return true;
}

int SshSubsystemFunction::on_data([[maybe_unused]] const void *data, [[maybe_unused]] size_t len)
{
    return 0;
}

void SshSubsystemFunction::on_resize([[maybe_unused]] const WinSize & winsize) {}

void SshSubsystemFunction::on_eof()
{
    SIHD_LOG(debug, "SshSubsystemFunction: EOF");
}

int SshSubsystemFunction::on_close()
{
    SIHD_LOG(debug, "SshSubsystemFunction: close");
    _channel = nullptr;
    return _exit_code;
}

const std::string & SshSubsystemFunction::command() const
{
    return _command;
}

int SshSubsystemFunction::stdout_fd() const
{
    return -1;
}

int SshSubsystemFunction::stderr_fd() const
{
    return -1;
}

SshChannel *SshSubsystemFunction::channel() const
{
    return _channel;
}

bool SshSubsystemFunction::is_running() const
{
    return !_unsent_out.empty() || !_unsent_err.empty();
}

bool SshSubsystemFunction::forward_output()
{
    if (_channel == nullptr)
        return false;
    const size_t unsent = _unsent_out.size() + _unsent_err.size();
    write_unsent(*_channel, _unsent_out);
    write_unsent_stderr(*_channel, _unsent_err);
    return _unsent_out.size() + _unsent_err.size() < unsent;
}

} // namespace sihd::ssh
