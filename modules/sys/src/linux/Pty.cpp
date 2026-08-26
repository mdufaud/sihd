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

#include <sihd/sys/Pty.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

// ============================================================================
// Platform detection
// ============================================================================

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

namespace sihd::sys
{

SIHD_LOGGER;

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

        // Configuration
        void set_shell(std::string_view shell) override;
        void set_args(std::vector<std::string> args) override;
        void set_env(std::string_view name, std::string_view value) override;
        void set_size(const PtySize & size) override;
        void set_working_directory(std::string_view path) override;

        // Lifecycle
        bool spawn() override;
        bool is_running() const override;
        int wait() override;
        void terminate() override;

        // I/O
        int read_fd() const override;
        ssize_t read(void *buffer, size_t size) override;
        ssize_t write(const void *data, size_t size) override;
        bool resize(const PtySize & size) override;
        void send_eof() override;

    private:
        std::string _shell;                                    ///< Shell executable path
        std::vector<std::string> _args;                        ///< Shell arguments
        std::vector<std::pair<std::string, std::string>> _env; ///< Environment variables
        std::string _working_dir;                              ///< Working directory
        PtySize _size;                                         ///< Terminal size

        pid_t _pid;       ///< Child process ID (-1 if not spawned)
        int _master_fd;   ///< Master side of the PTY (-1 if not spawned)
        int _exit_status; ///< Exit status (valid after wait())
        bool _exited;     ///< True if process has exited
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
    // Ensure child is terminated and resources are cleaned up
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
    _env.emplace_back(name, value);
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

    // Convert PtySize to struct winsize for forkpty()
    struct winsize ws;
    ws.ws_col = _size.cols;
    ws.ws_row = _size.rows;
    ws.ws_xpixel = _size.xpixel;
    ws.ws_ypixel = _size.ypixel;

    // forkpty() creates the PTY pair and forks in one call
    // - Parent: _master_fd is set, _pid is the child's PID
    // - Child: _pid is 0, stdin/stdout/stderr are connected to the slave
    _pid = forkpty(&_master_fd, nullptr, nullptr, &ws);

    if (_pid < 0)
    {
        SIHD_LOG(error, "PosixPty: forkpty failed: {}", strerror(errno));
        return false;
    }

    if (_pid == 0)
    {
        // ============================================
        // CHILD PROCESS - This code runs in the child
        // ============================================

        // Change working directory if specified
        if (!_working_dir.empty())
        {
            if (chdir(_working_dir.c_str()) != 0)
            {
                // Log to stderr since we're in the child
                fprintf(stderr, "chdir failed: %s\n", strerror(errno));
            }
        }

        // Set environment variables
        for (const auto & [name, value] : _env)
        {
            setenv(name.c_str(), value.c_str(), 1);
        }

        // Set TERM if not already set by the caller
        // xterm-256color provides good compatibility with most applications
        if (getenv("TERM") == nullptr)
        {
            setenv("TERM", "xterm-256color", 0);
        }

        // Build argv for execvp
        // argv[0] = shell name (basename for conventional shells)
        // argv[1..n] = arguments
        // argv[n+1] = nullptr (terminator)
        std::vector<char *> argv;

        // Use the shell path as argv[0]
        argv.push_back(const_cast<char *>(_shell.c_str()));

        // Add user-specified arguments
        for (auto & arg : _args)
        {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }

        // nullptr terminator required by execvp
        argv.push_back(nullptr);

        // Replace this process with the shell
        // execvp searches PATH for the executable
        execvp(_shell.c_str(), argv.data());

        // If we get here, exec failed
        fprintf(stderr, "exec %s failed: %s\n", _shell.c_str(), strerror(errno));
        _exit(127); // Standard exit code for command not found
    }

    // ============================================
    // PARENT PROCESS - Continue here
    // ============================================

    // Set master fd to non-blocking mode
    // This allows read() to return immediately if no data is available,
    // which is essential for integration with poll()/select() event loops
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

    // Use waitpid with WNOHANG for non-blocking check
    // Returns:
    // - 0: Child is still running
    // - _pid: Child has exited (status contains exit info)
    // - -1: Error
    int status;
    pid_t result = waitpid(_pid, &status, WNOHANG);

    if (result == _pid)
    {
        // Child has exited - update our state (const_cast needed for mutable state)
        auto *self = const_cast<PosixPty *>(this);
        self->_exited = true;

        if (WIFEXITED(status))
        {
            // Normal exit: extract exit code
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

    // Block until child exits
    int status;
    pid_t result = waitpid(_pid, &status, 0);

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

void PosixPty::terminate()
{
    if (_pid <= 0 || _exited)
        return;

    // Graceful termination sequence:
    // 1. SIGHUP - "Hangup" signal, shells handle this gracefully
    // 2. SIGTERM - Standard termination request
    // 3. SIGKILL - Force kill (cannot be caught or ignored)

    // Try SIGHUP first (what happens when terminal is closed)
    kill(_pid, SIGHUP);
    usleep(50000); // 50ms grace period

    if (is_running())
    {
        // SIGHUP didn't work, try SIGTERM
        kill(_pid, SIGTERM);
        usleep(100000); // 100ms grace period

        if (is_running())
        {
            // Last resort: SIGKILL
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

    ssize_t n = ::read(_master_fd, buffer, size);

    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        { // No data available (non-blocking mode)
            return 0;
        }
        // Real error
        return -1;
    }

    return n;
}

ssize_t PosixPty::write(const void *data, size_t size)
{
    if (_master_fd < 0)
        return -1;

    ssize_t n = ::write(_master_fd, data, size);

    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // Write would block
            return 0;
        }
        return -1;
    }

    return n;
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

    // TIOCSWINSZ sets the terminal size
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

// ============================================================================
// Factory functions for POSIX
// ============================================================================

bool Pty::is_supported()
{
    // POSIX systems always support PTY
    return true;
}

std::unique_ptr<Pty> Pty::create()
{
    return std::make_unique<PosixPty>();
}

} // namespace sihd::sys

#endif // SIHD_PTY_POSIX

// ============================================================================
// Unsupported platform (emscripten/web: no processes, no forkpty)
// ============================================================================

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
