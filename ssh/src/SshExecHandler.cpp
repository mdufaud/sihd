#include "sihd/ssh/utils.hpp"
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/proc.hpp>

#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshExecHandler.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

SshExecHandler::SshExecHandler(std::string_view command):
    _channel(nullptr),
    _command(command),
    _shell("/bin/sh"),
    _parse_mode(ParseMode::Shell),
    _fork_mode(false),
    _started(false),
    _exit_code(0),
    _pid(-1),
    _stdout_fd(-1),
    _stderr_fd(-1),
    _stdin_fd(-1),
    _has_pty(false),
    _winsize {}
{
    utils::init();
}

SshExecHandler::~SshExecHandler()
{
    if (_started)
        this->on_close();
    utils::finalize();
}

void SshExecHandler::set_exec_callback(ExecCallback callback)
{
    _exec_callback = std::move(callback);
}

void SshExecHandler::set_shell(std::string_view shell)
{
    _shell = shell;
}

void SshExecHandler::set_fork_mode(bool enable)
{
    _fork_mode = enable;
}

void SshExecHandler::set_parse_mode(ParseMode mode)
{
    _parse_mode = mode;
}

bool SshExecHandler::on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize)
{
    _channel = channel;
    _has_pty = has_pty;
    _winsize = winsize;

    SIHD_LOG(debug, "SshExecHandler: exec '{}' (fork={})", _command, _fork_mode);

    if (_fork_mode)
    {
        return start_fork_mode();
    }
    else
    {
        return start_callback_mode();
    }
}

bool SshExecHandler::start_callback_mode()
{
    _started = true;

    if (_exec_callback)
    {
        ExecResult result = _exec_callback(_command);
        _exit_code = result.exit_code;

        if (!result.output.empty() && _channel)
        {
            _channel->write(sihd::util::ArrCharView(result.output.data(), result.output.size()));
        }
    }
    else
    {
        // Default: use proc::execute to run the command
        std::string output;
        std::string errors;

        sihd::util::proc::Options opts;
        opts.stdout_callback = [&output](std::string_view data) {
            output += data;
        };
        opts.stderr_callback = [&errors](std::string_view data) {
            errors += data;
        };

        std::vector<std::string> args;
        if (_parse_mode == ParseMode::Shell)
        {
            // Use shell to parse the command
            args = {_shell, "-c", _command};
        }
        else
        {
            // Direct mode: parse with Splitter using shell-like rules
            sihd::util::Splitter splitter;
            splitter.set_delimiter_spaces();
            splitter.set_escape_sequences_all();
            args = splitter.split(_command);
        }

        auto future = sihd::util::proc::execute(args, opts);
        _exit_code = future.get();

        if (_channel)
        {
            if (!output.empty())
            {
                _channel->write(sihd::util::ArrCharView(output.data(), output.size()));
            }
            if (!errors.empty())
            {
                _channel->write_stderr(sihd::util::ArrCharView(errors.data(), errors.size()));
            }
        }
    }

    // Immediate completion for callback mode
    if (_channel)
    {
        _channel->request_send_exit_status(_exit_code);
        _channel->send_eof();
        _channel->close();
    }

    _started = false;
    return true;
}

bool SshExecHandler::start_fork_mode()
{
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0)
    {
        SIHD_LOG(error, "SshExecHandler: pipe failed: {}", strerror(errno));
        if (stdin_pipe[0] >= 0)
        {
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
        }
        if (stdout_pipe[0] >= 0)
        {
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
        }
        if (stderr_pipe[0] >= 0)
        {
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
        }
        return false;
    }

    _pid = fork();
    if (_pid == -1)
    {
        SIHD_LOG(error, "SshExecHandler: fork failed: {}", strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return false;
    }

    if (_pid == 0)
    {
        // Child process
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        execl(_shell.c_str(), _shell.c_str(), "-c", _command.c_str(), nullptr);
        _exit(127);
    }

    // Parent process
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    _stdin_fd = stdin_pipe[1];
    _stdout_fd = stdout_pipe[0];
    _stderr_fd = stderr_pipe[0];

    // Set non-blocking
    int flags = fcntl(_stdout_fd, F_GETFL, 0);
    fcntl(_stdout_fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(_stderr_fd, F_GETFL, 0);
    fcntl(_stderr_fd, F_SETFL, flags | O_NONBLOCK);

    _started = true;
    SIHD_LOG(debug, "SshExecHandler: forked pid={}", _pid);
    return true;
}

int SshExecHandler::on_data(const void *data, size_t len)
{
    if (!_started || _stdin_fd < 0)
        return 0;

    // Forward to child stdin
    ssize_t written = write(_stdin_fd, data, len);
    if (written < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        SIHD_LOG(error, "SshExecHandler: write failed: {}", strerror(errno));
        return -1;
    }

    return static_cast<int>(written);
}

void SshExecHandler::on_resize(const struct winsize & winsize)
{
    _winsize = winsize;
    // No PTY in basic exec mode, resize not applicable
}

void SshExecHandler::on_eof()
{
    SIHD_LOG(debug, "SshExecHandler: EOF from client");
    // Close stdin to child
    if (_stdin_fd >= 0)
    {
        close(_stdin_fd);
        _stdin_fd = -1;
    }
}

bool SshExecHandler::is_running() const
{
    if (!_started)
        return false;

    if (!_fork_mode)
        return false;

    if (_pid <= 0)
        return false;

    int status;
    pid_t ret = waitpid(_pid, &status, WNOHANG);
    if (ret == _pid)
    {
        // Store exit status
        auto *self = const_cast<SshExecHandler *>(this);
        if (WIFEXITED(status))
            self->_exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            self->_exit_code = 128 + WTERMSIG(status);
        self->_pid = -1;
        return false;
    }

    return (ret == 0);
}

void SshExecHandler::close_fds()
{
    if (_stdin_fd >= 0)
    {
        close(_stdin_fd);
        _stdin_fd = -1;
    }
    if (_stdout_fd >= 0)
    {
        close(_stdout_fd);
        _stdout_fd = -1;
    }
    if (_stderr_fd >= 0)
    {
        close(_stderr_fd);
        _stderr_fd = -1;
    }
}

int SshExecHandler::on_close()
{
    SIHD_LOG(debug, "SshExecHandler: close");

    if (_fork_mode && _pid > 0)
    {
        // Check if still running
        int status;
        pid_t ret = waitpid(_pid, &status, WNOHANG);
        if (ret == 0)
        {
            // Still running, kill it
            kill(_pid, SIGTERM);
            usleep(50000);
            ret = waitpid(_pid, &status, WNOHANG);
            if (ret == 0)
            {
                kill(_pid, SIGKILL);
                waitpid(_pid, &status, 0);
            }
        }

        if (WIFEXITED(status))
            _exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            _exit_code = 128 + WTERMSIG(status);

        _pid = -1;
    }

    close_fds();
    _started = false;
    _channel = nullptr;

    return _exit_code;
}

} // namespace sihd::ssh
