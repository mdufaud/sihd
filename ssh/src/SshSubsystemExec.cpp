#include <cerrno>
#include <cstring>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshSubsystemExec.hpp>
#include <sihd/ssh/utils.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

SshSubsystemExec::SshSubsystemExec(std::string_view command):
    _channel(nullptr),
    _command(command),
#if !defined(__SIHD_WINDOWS__)
    _shell("/bin/sh"),
#else
    _shell("cmd.exe"),
#endif
    _parse_mode(ParseMode::Shell),
    _fork_mode(true),
    _started(false),
    _exit_code(0),
    _has_pty(false),
    _winsize {}
{
    utils::init();
}

SshSubsystemExec::~SshSubsystemExec()
{
    if (_started)
        this->on_close();
    utils::finalize();
}

void SshSubsystemExec::set_shell(std::string_view shell)
{
    _shell = shell;
}

void SshSubsystemExec::set_fork_mode(bool enable)
{
    _fork_mode = enable;
}

void SshSubsystemExec::set_parse_mode(ParseMode mode)
{
    _parse_mode = mode;
}

void SshSubsystemExec::set_env(std::string_view name, std::string_view value)
{
    _env.emplace_back(name, value);
}

void SshSubsystemExec::set_working_directory(std::string_view path)
{
    _working_dir = path;
}

bool SshSubsystemExec::on_start(SshChannel *channel, bool has_pty, const WinSize & winsize)
{
    _channel = channel;
    _has_pty = has_pty;
    _winsize = winsize;

    SIHD_LOG(debug, "SshSubsystemExec: exec '{}' (fork={})", _command, _fork_mode);

    if (_fork_mode)
    {
        return start_process_mode();
    }
    else
    {
        return start_sync_mode();
    }
}

void SshSubsystemExec::configure_process_command()
{
    _process.clear_argv();
    if (_parse_mode == ParseMode::Shell)
    {
        _process.add_argv(_shell);
#if !defined(__SIHD_WINDOWS__)
        _process.add_argv("-c");
#else
        _process.add_argv("/c");
#endif
        _process.add_argv(_command);
    }
    else
    {
        sihd::util::Splitter splitter;
        splitter.set_delimiter_spaces();
        splitter.set_escape_sequences_all();
        auto args = splitter.split(_command);
        _process.add_argv(args);
    }

    for (const auto & [name, value] : _env)
    {
        _process.env_set(name, value);
    }

    if (!_working_dir.empty())
    {
        _process.set_chdir(_working_dir);
    }
}

bool SshSubsystemExec::start_sync_mode()
{
    configure_process_command();

    // Buffer child output; flushed to the channel on completion in on_close().
    _sync_out.clear();
    _sync_err.clear();
    _process.stdout_to(_sync_out);
    _process.stderr_to(_sync_err);

    if (!_process.execute())
    {
        SIHD_LOG(error, "SshSubsystemExec: failed to execute command");
        return false;
    }

    _started = true;
    SIHD_LOG(debug, "SshSubsystemExec: process started (buffered)");
    return true;
}

bool SshSubsystemExec::start_process_mode()
{
    configure_process_command();

    // Stream stdout/stderr to the channel incrementally
    _process.stdout_to([this](std::string_view data) {
        if (_channel)
            _channel->write(sihd::util::ArrCharView(data.data(), data.size()));
    });

    _process.stderr_to([this](std::string_view data) {
        if (_channel)
            _channel->write_stderr(sihd::util::ArrCharView(data.data(), data.size()));
    });

    if (!_process.execute())
    {
        SIHD_LOG(error, "SshSubsystemExec: failed to execute command");
        return false;
    }

    _started = true;
    SIHD_LOG(debug, "SshSubsystemExec: process started");
    return true;
}

int SshSubsystemExec::on_data(const void *data, size_t len)
{
    if (!_started)
        return 0;

    if (data == nullptr || len == 0)
        return 0;

    _process.stdin_from(std::string(static_cast<const char *>(data), len));
    return static_cast<int>(len);
}

void SshSubsystemExec::on_resize(const WinSize & winsize)
{
    _winsize = winsize;
}

void SshSubsystemExec::on_eof()
{
    SIHD_LOG(debug, "SshSubsystemExec: EOF from client");
    _process.stdin_close();
}

bool SshSubsystemExec::is_running() const
{
    if (!_started)
        return false;

    return _process.is_process_running();
}

bool SshSubsystemExec::forward_output()
{
    if (!_started)
        return false;

    if (!_process.can_read_pipes())
        return false;

    return _process.read_pipes(0);
}

int SshSubsystemExec::on_close()
{
    SIHD_LOG(debug, "SshSubsystemExec: close");

    if (_started)
    {
        // terminate() drains remaining pipes (filling the sync buffers), then
        // waits/reaps the child.
        _process.terminate();
        _exit_code = static_cast<int>(_process.return_code());

        // In buffered (non-fork) mode, flush the accumulated output now.
        if (!_fork_mode && _channel)
        {
            if (!_sync_out.empty())
                _channel->write(sihd::util::ArrCharView(_sync_out.data(), _sync_out.size()));
            if (!_sync_err.empty())
                _channel->write_stderr(sihd::util::ArrCharView(_sync_err.data(), _sync_err.size()));
        }
    }

    _started = false;
    _channel = nullptr;

    return _exit_code;
}

} // namespace sihd::ssh
