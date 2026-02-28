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
#include <sihd/ssh/SshSubsystemExec.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

SshSubsystemExec::SshSubsystemExec(std::string_view command):
    _channel(nullptr),
    _command(command),
    _shell("/bin/sh"),
    _parse_mode(ParseMode::Shell),
    _fork_mode(true),
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

bool SshSubsystemExec::on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize)
{
    _channel = channel;
    _has_pty = has_pty;
    _winsize = winsize;

    SIHD_LOG(debug, "SshSubsystemExec: exec '{}' (fork={})", _command, _fork_mode);

    if (_fork_mode)
    {
        return start_fork_mode();
    }
    else
    {
        return start_sync_mode();
    }
}

bool SshSubsystemExec::start_sync_mode()
{
    _started = true;

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
        args = {_shell, "-c", _command};
    }
    else
    {
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

    // Immediate completion for sync mode
    if (_channel)
    {
        _channel->request_send_exit_status(_exit_code);
        _channel->send_eof();
        _channel->close();
    }

    _started = false;
    return true;
}

bool SshSubsystemExec::start_fork_mode()
{
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0)
    {
        SIHD_LOG(error, "SshSubsystemExec: pipe failed: {}", strerror(errno));
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
        SIHD_LOG(error, "SshSubsystemExec: fork failed: {}", strerror(errno));
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

        // Set environment variables
        for (const auto & [name, value] : _env)
        {
            setenv(name.c_str(), value.c_str(), 1);
        }

        // Change working directory
        if (!_working_dir.empty())
        {
            if (chdir(_working_dir.c_str()) != 0)
            {
                _exit(127);
            }
        }

        if (_parse_mode == ParseMode::Shell)
        {
            execl(_shell.c_str(), _shell.c_str(), "-c", _command.c_str(), nullptr);
        }
        else
        {
            // Direct mode: split and exec
            sihd::util::Splitter splitter;
            splitter.set_delimiter_spaces();
            splitter.set_escape_sequences_all();
            auto args = splitter.split(_command);
            if (!args.empty())
            {
                std::vector<const char *> argv;
                for (const auto & a : args)
                    argv.push_back(a.c_str());
                argv.push_back(nullptr);
                execvp(argv[0], const_cast<char *const *>(argv.data()));
            }
        }
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
    SIHD_LOG(debug, "SshSubsystemExec: forked pid={}", _pid);
    return true;
}

int SshSubsystemExec::on_data(const void *data, size_t len)
{
    if (!_started || _stdin_fd < 0)
        return 0;

    ssize_t written = write(_stdin_fd, data, len);
    if (written < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        SIHD_LOG(error, "SshSubsystemExec: write failed: {}", strerror(errno));
        return -1;
    }

    return static_cast<int>(written);
}

void SshSubsystemExec::on_resize(const struct winsize & winsize)
{
    _winsize = winsize;
}

void SshSubsystemExec::on_eof()
{
    SIHD_LOG(debug, "SshSubsystemExec: EOF from client");
    if (_stdin_fd >= 0)
    {
        close(_stdin_fd);
        _stdin_fd = -1;
    }
}

bool SshSubsystemExec::is_running() const
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
        auto *self = const_cast<SshSubsystemExec *>(this);
        if (WIFEXITED(status))
            self->_exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            self->_exit_code = 128 + WTERMSIG(status);
        self->_pid = -1;
        return false;
    }

    return (ret == 0);
}

void SshSubsystemExec::close_fds()
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

int SshSubsystemExec::on_close()
{
    SIHD_LOG(debug, "SshSubsystemExec: close");

    if (_fork_mode && _pid > 0)
    {
        int status;
        pid_t ret = waitpid(_pid, &status, WNOHANG);
        if (ret == 0)
        {
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
