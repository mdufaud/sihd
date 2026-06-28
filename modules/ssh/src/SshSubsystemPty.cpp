#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshSubsystemPty.hpp>
#include <sihd/ssh/utils.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

SshSubsystemPty::SshSubsystemPty(): _channel(nullptr)
{
    utils::init();
}

SshSubsystemPty::~SshSubsystemPty()
{
    utils::finalize();
}

bool SshSubsystemPty::is_supported()
{
    return sys::Pty::is_supported();
}

void SshSubsystemPty::set_shell(std::string_view shell)
{
    _shell = shell;
}

void SshSubsystemPty::set_args(std::vector<std::string> args)
{
    _args = std::move(args);
}

void SshSubsystemPty::set_env(std::string_view name, std::string_view value)
{
    _env.emplace_back(name, value);
}

void SshSubsystemPty::set_working_directory(std::string_view path)
{
    _working_dir = path;
}

bool SshSubsystemPty::on_start(SshChannel *channel, bool has_pty, const WinSize & winsize)
{
    _channel = channel;

    // Check PTY support
    if (!sys::Pty::is_supported())
    {
        SIHD_LOG(error, "SshSubsystemPty: PTY not supported on this platform");
        return false;
    }

    // Create PTY
    _pty = sys::Pty::create();
    if (!_pty)
    {
        SIHD_LOG(error, "SshSubsystemPty: failed to create PTY");
        return false;
    }

    // Apply configuration
    if (!_shell.empty())
    {
        _pty->set_shell(_shell);
    }

    if (!_args.empty())
    {
        _pty->set_args(_args);
    }

    for (const auto & [name, value] : _env)
    {
        _pty->set_env(name, value);
    }

    if (!_working_dir.empty())
    {
        _pty->set_working_directory(_working_dir);
    }

    // Set terminal size
    sys::PtySize size;
    size.cols = winsize.ws_col;
    size.rows = winsize.ws_row;
    size.xpixel = winsize.ws_xpixel;
    size.ypixel = winsize.ws_ypixel;
    _pty->set_size(size);

    // Warn if no PTY was requested (shell may not work properly)
    if (!has_pty)
    {
        SIHD_LOG(warning, "SshSubsystemPty: client did not request PTY, shell may not work properly");
    }

    // Spawn the shell
    if (!_pty->spawn())
    {
        SIHD_LOG(error, "SshSubsystemPty: failed to spawn shell");
        _pty.reset();
        return false;
    }

    SIHD_LOG(debug, "SshSubsystemPty: started ({}x{})", winsize.ws_col, winsize.ws_row);
    return true;
}

void SshSubsystemPty::drain_pending_input()
{
    if (!_pty || _pending_in.empty())
        return;

    ssize_t written = _pty->write(_pending_in.data(), _pending_in.size());
    if (written > 0)
        _pending_in.erase(0, static_cast<size_t>(written));
}

int SshSubsystemPty::on_data(const void *data, size_t len)
{
    if (!_pty)
        return 0;

    if (data == nullptr || len == 0)
        return 0;

    // Preserve ordering: if a previous partial write left bytes buffered, append
    // the new data behind them and drain in order. libssh's data callback always
    // reports all bytes consumed, so unwritten bytes must be kept locally to
    // avoid data loss.
    if (!_pending_in.empty())
    {
        _pending_in.append(static_cast<const char *>(data), len);
        this->drain_pending_input();
        return static_cast<int>(len);
    }

    ssize_t written = _pty->write(data, len);
    if (written < 0)
    {
        SIHD_LOG(error, "SshSubsystemPty: write to PTY failed");
        return -1;
    }

    if (static_cast<size_t>(written) < len)
    {
        const char *tail = static_cast<const char *>(data) + written;
        _pending_in.append(tail, len - static_cast<size_t>(written));
    }

    return static_cast<int>(len);
}

void SshSubsystemPty::on_resize(const WinSize & winsize)
{
    if (!_pty)
        return;

    sys::PtySize size;
    size.cols = winsize.ws_col;
    size.rows = winsize.ws_row;
    size.xpixel = winsize.ws_xpixel;
    size.ypixel = winsize.ws_ypixel;

    if (_pty->resize(size))
    {
        SIHD_LOG(debug, "SshSubsystemPty: resized to {}x{}", winsize.ws_col, winsize.ws_row);
    }
    else
    {
        SIHD_LOG(warning, "SshSubsystemPty: resize failed");
    }
}

void SshSubsystemPty::on_eof()
{
    SIHD_LOG(debug, "SshSubsystemPty: EOF from client");

    if (_pty)
    {
        _pty->send_eof();
    }
}

int SshSubsystemPty::on_close()
{
    SIHD_LOG(debug, "SshSubsystemPty: closing");

    if (!_pty)
        return -1;

    // Terminate the shell process gracefully
    _pty->terminate();

    // Wait for exit and get exit code
    int exit_code = _pty->wait();

    SIHD_LOG(debug, "SshSubsystemPty: shell exited with code {}", exit_code);

    // Clean up
    _pty.reset();

    return exit_code;
}

int SshSubsystemPty::stdout_fd() const
{
    if (!_pty)
        return -1;

    return _pty->read_fd();
}

bool SshSubsystemPty::is_running() const
{
    if (!_pty)
        return false;

    return _pty->is_running();
}

bool SshSubsystemPty::forward_output()
{
    if (!_pty || !_channel)
        return false;

    // Flush any client input the PTY could not accept earlier before reading.
    this->drain_pending_input();

    if (_read_buf.byte_capacity() < 4096 && !_read_buf.reserve(4096))
        return false;

    ssize_t n = _pty->read(_read_buf.buf(), _read_buf.byte_capacity());
    if (n > 0)
    {
        _channel->write(
            sihd::util::ArrCharView(reinterpret_cast<const char *>(_read_buf.buf()), static_cast<size_t>(n)));
        return true;
    }
    return false;
}

} // namespace sihd::ssh
