/**
 * @file Pty.cpp
 * @brief POSIX PTY (pseudo-terminal) implementation.
 *
 * **PosixPty** covers Linux, macOS, BSD, and other POSIX-compliant systems.
 * Uses forkpty() to create a pseudo-terminal and fork a child process.
 * The Windows ConPTY implementation lives in src/windows/Pty.cpp; emscripten
 * compiles the no-op stub at the bottom of this file.
 *
 * ## References
 * - POSIX PTY: https://man7.org/linux/man-pages/man3/forkpty.3.html
 *
 * @copyright MIT License
 */

#include <cstring>

#include <sihd/sys/Environment.hpp>
#include <sihd/sys/Pty.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#if defined(__SIHD_EMSCRIPTEN__)
# define SIHD_PTY_UNSUPPORTED 1
#else
# define SIHD_PTY_POSIX 1
#endif
#if defined(SIHD_PTY_POSIX)

# include <fcntl.h>
# include <signal.h>
# include <sys/ioctl.h>
# include <sys/wait.h>
# include <termios.h>
# include <unistd.h>

# include <cerrno>
# include <cstdlib>

// forkpty() is in different headers on different systems
# if defined(__linux__)
#  include <pty.h>
# elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#  include <util.h>
# endif

# include "internal/environment.hpp"

namespace sihd::sys
{

SIHD_LOGGER;

namespace
{

// forked child: the logger may be locked by another thread of the parent
void child_write_stderr(const std::string & message)
{
    [[maybe_unused]] const ssize_t written = ::write(STDERR_FILENO, message.data(), message.size());
}

} // namespace

/**
 * @brief POSIX PTY implementation using forkpty().
 *
 * ## How it works
 *
 * 1. **forkpty()** creates a pseudo-terminal pair:
 *    - Master side: Used by this process for reading/writing
 *    - Slave side: Becomes the child's controlling terminal
 *
 * 2. The call also **forks** the process:
 *    - Parent: Receives master fd, continues execution
 *    - Child: Has slave as stdin/stdout/stderr, execs the shell
 *
 * 3. Data flow:
 *    ```
 *    [This Process] <--master fd--> [PTY] <--slave--> [Shell Process]
 *    ```
 *
 * 4. Terminal size changes:
 *    - Use ioctl(TIOCSWINSZ) to set size on master fd
 *    - Kernel sends SIGWINCH to child automatically
 *
 * ## Non-blocking I/O
 * The master fd is set to non-blocking mode so read() returns immediately
 * if no data is available, instead of blocking the caller.
 */
class PosixPty: public Pty
{
    public:
        PosixPty();
        ~PosixPty() override;

        void set_shell(std::string_view shell) override;
        void set_args(std::vector<std::string> args) override;
        void set_env(std::string_view name, std::string_view value) override;
        void set_size(const PtySize & size) override;
        void set_working_directory(std::string_view path) override;

        bool spawn() override;
        bool is_running() const override;
        int wait() override;
        void terminate() override;

        int read_fd() const override;
        ssize_t read(void *buffer, size_t size) override;
        ssize_t write(const void *data, size_t size) override;
        bool resize(const PtySize & size) override;
        void send_eof() override;

    private:
        pid_t _wait_child(int & status, int options) const;
        std::string _shell;
        std::vector<std::string> _args;
        Environment _env;
        std::string _working_dir;
        PtySize _size;

        pid_t _pid;
        int _master_fd;
        int _exit_status;
        bool _exited;
};

PosixPty::PosixPty():
    _shell("/bin/sh"),
    _size {80, 24, 0, 0},
    _pid(-1),
    _master_fd(-1),
    _exit_status(-1),
    _exited(false)
{
}

PosixPty::~PosixPty()
{
    if (_pid > 0 && !_exited)
    {
        terminate();
        wait();
    }

    if (_master_fd >= 0)
    {
        ::close(_master_fd);
        _master_fd = -1;
    }
}

void PosixPty::set_shell(std::string_view shell)
{
    _shell = shell;
}

void PosixPty::set_args(std::vector<std::string> args)
{
    _args = std::move(args);
}

void PosixPty::set_env(std::string_view name, std::string_view value)
{
    _env.set(name, value);
}

void PosixPty::set_size(const PtySize & size)
{
    _size = size;
}

void PosixPty::set_working_directory(std::string_view path)
{
    _working_dir = path;
}

bool PosixPty::spawn()
{
    if (_pid > 0)
    {
        SIHD_LOG(error, "PosixPty: already spawned");
        return false;
    }

    struct winsize ws;
    ws.ws_col = _size.cols;
    ws.ws_row = _size.rows;
    ws.ws_xpixel = _size.xpixel;
    ws.ws_ypixel = _size.ypixel;

    // parent: _master_fd set, _pid = child; child: std streams on the slave, _pid == 0
    _pid = forkpty(&_master_fd, nullptr, nullptr, &ws);

    if (_pid < 0)
    {
        SIHD_LOG(error, "PosixPty: forkpty failed: {}", strerror(errno));
        return false;
    }

    if (_pid == 0)
    {
        if (!_working_dir.empty())
        {
            if (chdir(_working_dir.c_str()) != 0)
            {
                child_write_stderr(std::string("chdir failed: ") + strerror(errno) + "\n");
            }
        }

        if (auto applied = internal::apply(_env); !applied)
            child_write_stderr("failed to apply environment: " + applied.error().message + "\n");

        if (env::get("TERM").has_value() == false)
        {
            if (auto term = env::set("TERM", "xterm-256color"); !term)
                child_write_stderr("failed to set TERM: " + term.error().message + "\n");
        }

        std::vector<char *> argv;

        argv.push_back(const_cast<char *>(_shell.c_str()));

        for (auto & arg : _args)
        {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }

        argv.push_back(nullptr);

        execvp(_shell.c_str(), argv.data());

        child_write_stderr("exec " + _shell + " failed: " + strerror(errno) + "\n");
        _exit(127); // Standard exit code for command not found
    }

    // master fd non-blocking: read() returns immediately with no data, required by poll() loops
    int flags = fcntl(_master_fd, F_GETFL, 0);
    if (flags != -1)
    {
        fcntl(_master_fd, F_SETFL, flags | O_NONBLOCK);
    }

    SIHD_LOG(debug, "PosixPty: spawned '{}' (pid={}, master_fd={})", _shell, _pid, _master_fd);
    return true;
}

bool PosixPty::is_running() const
{
    if (_pid <= 0 || _exited)
        return false;

    // waitpid(WNOHANG): 0 = still running, _pid = exited (status set), -1 = error
    int status;
    const pid_t result = this->_wait_child(status, WNOHANG);

    if (result == _pid)
    {
        auto *self = const_cast<PosixPty *>(this);
        self->_exited = true;

        if (WIFEXITED(status))
        {
            self->_exit_status = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            // Killed by signal: convention is 128 + signal number
            self->_exit_status = 128 + WTERMSIG(status);
        }

        return false;
    }

    return (result == 0);
}

int PosixPty::wait()
{
    if (_pid <= 0)
        return -1;

    if (_exited)
        return _exit_status;

    int status;
    const pid_t result = this->_wait_child(status, 0);

    if (result == _pid)
    {
        _exited = true;
        if (WIFEXITED(status))
        {
            _exit_status = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            _exit_status = 128 + WTERMSIG(status);
        }
    }

    return _exit_status;
}

pid_t PosixPty::_wait_child(int & status, int options) const
{
    while (true)
    {
        const pid_t result = waitpid(_pid, &status, options);
        if (result >= 0 || errno != EINTR)
            return result;
    }
}

void PosixPty::terminate()
{
    if (_pid <= 0 || _exited)
        return;

    // graceful termination: SIGHUP (shells handle it), then SIGTERM, then SIGKILL

    kill(_pid, SIGHUP);
    usleep(50000);

    if (is_running())
    {
        kill(_pid, SIGTERM);
        usleep(100000);

        if (is_running())
        {
            kill(_pid, SIGKILL);
        }
    }
}

int PosixPty::read_fd() const
{
    return _master_fd;
}

ssize_t PosixPty::read(void *buffer, size_t size)
{
    if (_master_fd < 0)
        return -1;

    ssize_t bytes = ::read(_master_fd, buffer, size);

    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        { // No data available (non-blocking mode)
            return 0;
        }
        return -1;
    }

    return bytes;
}

ssize_t PosixPty::write(const void *data, size_t size)
{
    if (_master_fd < 0)
        return -1;

    ssize_t bytes = ::write(_master_fd, data, size);

    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        return -1;
    }

    return bytes;
}

bool PosixPty::resize(const PtySize & size)
{
    if (_master_fd < 0)
        return false;

    _size = size;

    struct winsize ws;
    ws.ws_col = size.cols;
    ws.ws_row = size.rows;
    ws.ws_xpixel = size.xpixel;
    ws.ws_ypixel = size.ypixel;

    // The kernel will automatically send SIGWINCH to the foreground process group
    if (ioctl(_master_fd, TIOCSWINSZ, &ws) < 0)
    {
        SIHD_LOG(warning, "PosixPty: ioctl TIOCSWINSZ failed: {}", strerror(errno));
        return false;
    }

    return true;
}

void PosixPty::send_eof()
{
    if (_master_fd >= 0)
    {
        // Closing the master fd signals EOF to the slave
        ::close(_master_fd);
        _master_fd = -1;
    }
}

bool Pty::is_supported()
{
    return true;
}

std::unique_ptr<Pty> Pty::create()
{
    return std::make_unique<PosixPty>();
}

} // namespace sihd::sys

#endif // SIHD_PTY_POSIX

#if defined(SIHD_PTY_UNSUPPORTED)

namespace sihd::sys
{

bool Pty::is_supported()
{
    return false;
}

std::unique_ptr<Pty> Pty::create()
{
    return nullptr;
}

} // namespace sihd::sys

#endif // SIHD_PTY_UNSUPPORTED
