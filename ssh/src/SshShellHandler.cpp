#include <cstring>

#include <sihd/util/Logger.hpp>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshShellHandler.hpp>
#include <sihd/ssh/utils.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

SshShellHandler::SshShellHandler():
    _channel(nullptr),
    _running(false),
    _has_pty(false),
    _winsize {},
    _eof_byte(0x04), // Ctrl-D
    _handle_eof(true)
{
    utils::init();
}

SshShellHandler::~SshShellHandler()
{
    if (_running)
        this->on_close();
    utils::finalize();
}

void SshShellHandler::set_welcome_message(std::string_view message)
{
    _welcome_message = message;
}

void SshShellHandler::set_goodbye_message(std::string_view message)
{
    _goodbye_message = message;
}

void SshShellHandler::set_eof_byte(uint8_t byte)
{
    _eof_byte = byte;
}

void SshShellHandler::set_handle_eof(bool enable)
{
    _handle_eof = enable;
}

void SshShellHandler::set_data_callback(DataCallback callback)
{
    _data_callback = std::move(callback);
}

void SshShellHandler::set_resize_callback(ResizeCallback callback)
{
    _resize_callback = std::move(callback);
}

bool SshShellHandler::on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize)
{
    _channel = channel;
    _has_pty = has_pty;
    _winsize = winsize;
    _running = true;

    SIHD_LOG(debug, "SshShellHandler: started (pty={}, {}x{})", has_pty, winsize.ws_col, winsize.ws_row);

    // Send welcome message
    if (!_welcome_message.empty() && _channel)
    {
        _channel->write(sihd::util::ArrCharView(_welcome_message.data(), _welcome_message.size()));
    }

    return true;
}

int SshShellHandler::on_data(const void *data, size_t len)
{
    if (!_running || !_channel || len == 0)
        return 0;

    const uint8_t *bytes = static_cast<const uint8_t *>(data);

    // Check for EOF
    if (_handle_eof)
    {
        for (size_t i = 0; i < len; i++)
        {
            if (bytes[i] == _eof_byte)
            {
                SIHD_LOG(debug, "SshShellHandler: EOF received");
                this->on_eof();
                return static_cast<int>(i + 1);
            }
        }
    }

    // Use custom callback or default echo
    if (_data_callback)
    {
        return _data_callback(_channel, data, len);
    }

    // Default: echo back
    _channel->write(sihd::util::ArrCharView(static_cast<const char *>(data), len));
    return static_cast<int>(len);
}

void SshShellHandler::on_resize(const struct winsize & winsize)
{
    _winsize = winsize;
    SIHD_LOG(debug, "SshShellHandler: resize {}x{}", winsize.ws_col, winsize.ws_row);

    if (_resize_callback)
    {
        _resize_callback(winsize);
    }
}

void SshShellHandler::on_eof()
{
    if (!_running)
        return;

    SIHD_LOG(debug, "SshShellHandler: EOF");
    _running = false;

    if (_channel)
    {
        // Send goodbye message
        if (!_goodbye_message.empty())
        {
            _channel->write(sihd::util::ArrCharView(_goodbye_message.data(), _goodbye_message.size()));
        }

        _channel->request_send_exit_status(0);
        _channel->send_eof();
        _channel->close();
    }
}

int SshShellHandler::on_close()
{
    SIHD_LOG(debug, "SshShellHandler: close");
    _running = false;
    _channel = nullptr;
    return 0;
}

} // namespace sihd::ssh
