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
    return Pty::is_supported();
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

bool SshSubsystemPty::on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize)
{
    _channel = channel;

    // Check PTY support
    if (!Pty::is_supported())
    {
        SIHD_LOG(error, "SshSubsystemPty: PTY not supported on this platform");
        return false;
    }

    // Create PTY
    _pty = Pty::create();
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
    PtySize size;
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

int SshSubsystemPty::on_data(const void *data, size_t len)
{
    if (!_pty)
        return 0;

    // Write data from SSH client to PTY input (goes to shell's stdin)
    ssize_t written = _pty->write(data, len);

    if (written < 0)
    {
        SIHD_LOG(error, "SshSubsystemPty: write to PTY failed");
        return -1;
    }

    return static_cast<int>(written);
}

void SshSubsystemPty::on_resize(const struct winsize & winsize)
{
    if (!_pty)
        return;

    PtySize size;
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

} // namespace sihd::ssh
