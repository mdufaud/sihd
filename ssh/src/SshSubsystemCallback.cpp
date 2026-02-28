#include <sihd/util/Logger.hpp>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshSubsystemCallback.hpp>
#include <sihd/ssh/utils.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

SshSubsystemCallback::SshSubsystemCallback(std::string_view command):
    _channel(nullptr),
    _command(command),
    _exit_code(0)
{
    utils::init();
}

SshSubsystemCallback::~SshSubsystemCallback()
{
    utils::finalize();
}

void SshSubsystemCallback::set_callback(Callback callback)
{
    _callback = std::move(callback);
}

bool SshSubsystemCallback::on_start(SshChannel *channel,
                                    [[maybe_unused]] bool has_pty,
                                    [[maybe_unused]] const struct winsize & winsize)
{
    _channel = channel;

    if (!_callback)
    {
        SIHD_LOG(error, "SshSubsystemCallback: no callback set for command '{}'", _command);
        return false;
    }

    SIHD_LOG(debug, "SshSubsystemCallback: exec '{}'", _command);

    Result result = _callback(_command);
    _exit_code = result.exit_code;

    if (_channel)
    {
        if (!result.output.empty())
        {
            _channel->write(sihd::util::ArrCharView(result.output.data(), result.output.size()));
        }
        if (!result.error_output.empty())
        {
            _channel->write_stderr(
                sihd::util::ArrCharView(result.error_output.data(), result.error_output.size()));
        }
        // exit_status + eof + close are handled by BasicSshServerHandler::on_poll()
        // via on_close() return value, since manages_channel_io() is false.
    }

    return true;
}

int SshSubsystemCallback::on_data([[maybe_unused]] const void *data, [[maybe_unused]] size_t len)
{
    // Callback mode is synchronous - no data expected after start
    return 0;
}

void SshSubsystemCallback::on_resize([[maybe_unused]] const struct winsize & winsize)
{
    // No-op: callback mode doesn't use PTY
}

void SshSubsystemCallback::on_eof()
{
    SIHD_LOG(debug, "SshSubsystemCallback: EOF");
}

int SshSubsystemCallback::on_close()
{
    SIHD_LOG(debug, "SshSubsystemCallback: close");
    _channel = nullptr;
    return _exit_code;
}

} // namespace sihd::ssh
