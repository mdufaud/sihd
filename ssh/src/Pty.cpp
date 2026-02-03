/**
 * @file Pty.cpp
 * @brief Cross-platform PTY (pseudo-terminal) implementation.
 *
 * This file contains two implementations:
 * 1. **PosixPty**: For Linux, macOS, BSD, and other POSIX-compliant systems.
 *    Uses forkpty() to create a pseudo-terminal and fork a child process.
 *
 * 2. **ConPty**: For Windows 10 version 1809 (build 17763) and later.
 *    Uses the Windows Pseudo Console API (CreatePseudoConsole).
 *
 * ## Platform Detection
 * - _WIN32: Windows (uses ConPty)
 * - Everything else: POSIX (uses PosixPty)
 *
 * ## References
 * - POSIX PTY: https://man7.org/linux/man-pages/man3/forkpty.3.html
 * - Windows ConPTY: https://docs.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session
 * - Windows Terminal sample: https://github.com/microsoft/terminal/tree/main/samples/ConPTY
 *
 * @copyright MIT License
 */

#include <sihd/ssh/Pty.hpp>
#include <sihd/util/Logger.hpp>

#include <cstring>

// ============================================================================
// Platform detection
// ============================================================================

#if defined(_WIN32)
# define SIHD_PTY_WINDOWS 1
#else
# define SIHD_PTY_POSIX 1
#endif

// ============================================================================
// POSIX Implementation (Linux, macOS, BSD, etc.)
// ============================================================================

#if defined(SIHD_PTY_POSIX)

# include <cerrno>
# include <cstdlib>
# include <fcntl.h>
# include <signal.h>
# include <sys/ioctl.h>
# include <sys/wait.h>
# include <termios.h>
# include <unistd.h>

// forkpty() is in different headers on different systems
# if defined(__linux__)
#  include <pty.h>
# elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#  include <util.h>
# endif

namespace sihd::ssh
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

} // namespace sihd::ssh

#endif // SIHD_PTY_POSIX

// ============================================================================
// Windows Implementation (ConPTY)
// ============================================================================

#if defined(SIHD_PTY_WINDOWS)

# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h>

namespace sihd::ssh
{

SIHD_NEW_LOGGER("sihd::ssh::pty");

/**
 * @brief Windows ConPTY implementation.
 *
 * ## How it works
 *
 * ConPTY (Console Pseudo Terminal) is a Windows API introduced in Windows 10
 * version 1809 (build 17763). It provides similar functionality to POSIX PTY.
 *
 * 1. **CreatePseudoConsole()** creates the pseudo console with two pipes:
 *    - Input pipe: Data written here goes to the console's stdin
 *    - Output pipe: Console output (stdout/stderr) can be read here
 *
 * 2. **CreateProcess()** with PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE spawns
 *    a process attached to the pseudo console.
 *
 * 3. Data flow:
 *    ```
 *    [This Process] --write--> [Input Pipe] --> [ConPTY] --> [Shell stdin]
 *    [This Process] <--read-- [Output Pipe] <-- [ConPTY] <-- [Shell stdout/stderr]
 *    ```
 *
 * 4. Terminal size changes:
 *    - Use ResizePseudoConsole() API
 *
 * ## Limitations
 * - Requires Windows 10 1809 or later
 * - No support for older Windows versions
 * - File descriptors are actually HANDLEs (use WaitForSingleObject, not select)
 *
 * ## References
 * - https://docs.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session
 * - https://github.com/microsoft/terminal/tree/main/samples/ConPTY
 */
class ConPty: public Pty
{
    public:
        ConPty();
        ~ConPty() override;

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
        std::string _shell;                                    ///< Shell executable
        std::vector<std::string> _args;                        ///< Shell arguments
        std::vector<std::pair<std::string, std::string>> _env; ///< Environment variables
        std::string _working_dir;                              ///< Working directory
        PtySize _size;                                         ///< Terminal size

        HPCON _hpc;                              ///< Pseudo console handle
        HANDLE _pipe_in_read;                    ///< Pipe: PTY reads from here (we write to _pipe_in_write)
        HANDLE _pipe_in_write;                   ///< Pipe: We write input here
        HANDLE _pipe_out_read;                   ///< Pipe: We read output from here
        HANDLE _pipe_out_write;                  ///< Pipe: PTY writes to here
        HANDLE _process;                         ///< Child process handle
        HANDLE _thread;                          ///< Child main thread handle
        PROCESS_INFORMATION _pi;                 ///< Process information
        LPPROC_THREAD_ATTRIBUTE_LIST _attr_list; ///< Thread attribute list for ConPTY
        bool _spawned;
        int _exit_status;
};

// Function pointers for dynamic loading (to support running on older Windows)
typedef HRESULT(WINAPI *PFN_CreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD, HPCON *);
typedef HRESULT(WINAPI *PFN_ResizePseudoConsole)(HPCON, COORD);
typedef void(WINAPI *PFN_ClosePseudoConsole)(HPCON);

static PFN_CreatePseudoConsole pfnCreatePseudoConsole = nullptr;
static PFN_ResizePseudoConsole pfnResizePseudoConsole = nullptr;
static PFN_ClosePseudoConsole pfnClosePseudoConsole = nullptr;
static bool s_conpty_checked = false;
static bool s_conpty_available = false;

/**
 * @brief Check if ConPTY is available on this system.
 *
 * ConPTY was introduced in Windows 10 version 1809 (build 17763).
 * We dynamically load the functions to support running on older systems
 * (the executable won't crash, it will just report PTY as unsupported).
 */
static bool CheckConPtySupport()
{
    if (s_conpty_checked)
        return s_conpty_available;

    s_conpty_checked = true;

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32)
    {
        SIHD_LOG(debug, "ConPty: kernel32.dll not found");
        return false;
    }

    pfnCreatePseudoConsole = (PFN_CreatePseudoConsole)GetProcAddress(kernel32, "CreatePseudoConsole");
    pfnResizePseudoConsole = (PFN_ResizePseudoConsole)GetProcAddress(kernel32, "ResizePseudoConsole");
    pfnClosePseudoConsole = (PFN_ClosePseudoConsole)GetProcAddress(kernel32, "ClosePseudoConsole");

    if (pfnCreatePseudoConsole && pfnResizePseudoConsole && pfnClosePseudoConsole)
    {
        s_conpty_available = true;
        SIHD_LOG(debug, "ConPty: API available (Windows 10 1809+)");
    }
    else
    {
        SIHD_LOG(debug, "ConPty: API not available (requires Windows 10 1809+)");
    }

    return s_conpty_available;
}

ConPty::ConPty():
    _shell("cmd.exe"),
    _size {80, 24, 0, 0},
    _hpc(nullptr),
    _pipe_in_read(INVALID_HANDLE_VALUE),
    _pipe_in_write(INVALID_HANDLE_VALUE),
    _pipe_out_read(INVALID_HANDLE_VALUE),
    _pipe_out_write(INVALID_HANDLE_VALUE),
    _process(INVALID_HANDLE_VALUE),
    _thread(INVALID_HANDLE_VALUE),
    _attr_list(nullptr),
    _spawned(false),
    _exit_status(-1)
{
    ZeroMemory(&_pi, sizeof(_pi));
}

ConPty::~ConPty()
{
    if (_spawned)
    {
        terminate();
        wait();
    }

    // Close pseudo console first
    if (_hpc && pfnClosePseudoConsole)
    {
        pfnClosePseudoConsole(_hpc);
        _hpc = nullptr;
    }

    // Close pipes
    if (_pipe_in_read != INVALID_HANDLE_VALUE)
        CloseHandle(_pipe_in_read);
    if (_pipe_in_write != INVALID_HANDLE_VALUE)
        CloseHandle(_pipe_in_write);
    if (_pipe_out_read != INVALID_HANDLE_VALUE)
        CloseHandle(_pipe_out_read);
    if (_pipe_out_write != INVALID_HANDLE_VALUE)
        CloseHandle(_pipe_out_write);

    // Close process handles
    if (_pi.hProcess != INVALID_HANDLE_VALUE && _pi.hProcess != nullptr)
        CloseHandle(_pi.hProcess);
    if (_pi.hThread != INVALID_HANDLE_VALUE && _pi.hThread != nullptr)
        CloseHandle(_pi.hThread);

    // Free attribute list
    if (_attr_list)
    {
        DeleteProcThreadAttributeList(_attr_list);
        HeapFree(GetProcessHeap(), 0, _attr_list);
        _attr_list = nullptr;
    }
}

void ConPty::set_shell(std::string_view shell)
{
    _shell = shell;
}

void ConPty::set_args(std::vector<std::string> args)
{
    _args = std::move(args);
}

void ConPty::set_env(std::string_view name, std::string_view value)
{
    _env.emplace_back(name, value);
}

void ConPty::set_size(const PtySize & size)
{
    _size = size;
}

void ConPty::set_working_directory(std::string_view path)
{
    _working_dir = path;
}

bool ConPty::spawn()
{
    if (_spawned)
    {
        SIHD_LOG(error, "ConPty: already spawned");
        return false;
    }

    if (!CheckConPtySupport())
    {
        SIHD_LOG(error, "ConPty: not supported on this Windows version");
        return false;
    }

    // Create pipes for PTY input/output
    // Input pipe: we write to _pipe_in_write, PTY reads from _pipe_in_read
    // Output pipe: PTY writes to _pipe_out_write, we read from _pipe_out_read
    if (!CreatePipe(&_pipe_in_read, &_pipe_in_write, nullptr, 0))
    {
        SIHD_LOG(error, "ConPty: failed to create input pipe");
        return false;
    }

    if (!CreatePipe(&_pipe_out_read, &_pipe_out_write, nullptr, 0))
    {
        SIHD_LOG(error, "ConPty: failed to create output pipe");
        return false;
    }

    // Create the pseudo console
    COORD size = {static_cast<SHORT>(_size.cols), static_cast<SHORT>(_size.rows)};
    HRESULT hr = pfnCreatePseudoConsole(size, _pipe_in_read, _pipe_out_write, 0, &_hpc);
    if (FAILED(hr))
    {
        SIHD_LOG(error, "ConPty: CreatePseudoConsole failed: 0x{:08x}", static_cast<unsigned>(hr));
        return false;
    }

    // Close the pipe ends that the ConPTY owns (they're duplicated internally)
    CloseHandle(_pipe_in_read);
    _pipe_in_read = INVALID_HANDLE_VALUE;
    CloseHandle(_pipe_out_write);
    _pipe_out_write = INVALID_HANDLE_VALUE;

    // Create process with the pseudo console attached
    // We need to use EXTENDED_STARTUPINFO_PRESENT and set the PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE

    // First, determine the size of the attribute list
    SIZE_T attr_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);

    _attr_list = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attr_size);
    if (!_attr_list)
    {
        SIHD_LOG(error, "ConPty: failed to allocate attribute list");
        return false;
    }

    if (!InitializeProcThreadAttributeList(_attr_list, 1, 0, &attr_size))
    {
        SIHD_LOG(error, "ConPty: InitializeProcThreadAttributeList failed");
        return false;
    }

    // Associate the pseudo console with the process
    if (!UpdateProcThreadAttribute(_attr_list,
                                   0,
                                   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   _hpc,
                                   sizeof(_hpc),
                                   nullptr,
                                   nullptr))
    {
        SIHD_LOG(error, "ConPty: UpdateProcThreadAttribute failed");
        return false;
    }

    // Build command line
    std::string cmdline = _shell;
    for (const auto & arg : _args)
    {
        cmdline += " ";
        // Quote arguments containing spaces
        if (arg.find(' ') != std::string::npos)
        {
            cmdline += "\"" + arg + "\"";
        }
        else
        {
            cmdline += arg;
        }
    }

    // Build environment block (optional)
    // For simplicity, we'll inherit the parent's environment and just set TERM
    // A full implementation would build a complete environment block

    // Create the process
    STARTUPINFOEXA si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXA);
    si.lpAttributeList = _attr_list;

    std::vector<char> cmdline_buf(cmdline.begin(), cmdline.end());
    cmdline_buf.push_back('\0');

    LPCSTR working_dir = _working_dir.empty() ? nullptr : _working_dir.c_str();

    if (!CreateProcessA(nullptr,
                        cmdline_buf.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        EXTENDED_STARTUPINFO_PRESENT,
                        nullptr, // Inherit environment
                        working_dir,
                        &si.StartupInfo,
                        &_pi))
    {
        SIHD_LOG(error, "ConPty: CreateProcess failed: {}", GetLastError());
        return false;
    }

    _spawned = true;
    SIHD_LOG(debug, "ConPty: spawned '{}' (pid={})", _shell, _pi.dwProcessId);
    return true;
}

bool ConPty::is_running() const
{
    if (!_spawned)
        return false;

    DWORD exit_code;
    if (GetExitCodeProcess(_pi.hProcess, &exit_code))
    {
        if (exit_code != STILL_ACTIVE)
        {
            auto *self = const_cast<ConPty *>(this);
            self->_exit_status = static_cast<int>(exit_code);
            return false;
        }
        return true;
    }

    return false;
}

int ConPty::wait()
{
    if (!_spawned)
        return -1;

    WaitForSingleObject(_pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(_pi.hProcess, &exit_code);
    _exit_status = static_cast<int>(exit_code);

    return _exit_status;
}

void ConPty::terminate()
{
    if (!_spawned)
        return;

    // Windows doesn't have a graceful termination signal like SIGTERM
    // We directly terminate the process
    TerminateProcess(_pi.hProcess, 1);
}

int ConPty::read_fd() const
{
    // On Windows, we return -1 since the HANDLE can't be used with select()
    // Callers should use WaitForSingleObject with the actual handle
    // For now, we return a placeholder (this would need platform-specific handling)
    return _pipe_out_read != INVALID_HANDLE_VALUE ? 1 : -1;
}

ssize_t ConPty::read(void *buffer, size_t size)
{
    if (_pipe_out_read == INVALID_HANDLE_VALUE)
        return -1;

    DWORD bytes_available = 0;
    if (!PeekNamedPipe(_pipe_out_read, nullptr, 0, nullptr, &bytes_available, nullptr))
    {
        return -1;
    }

    if (bytes_available == 0)
    {
        return 0; // No data available (non-blocking behavior)
    }

    DWORD bytes_read = 0;
    if (!ReadFile(_pipe_out_read, buffer, static_cast<DWORD>(size), &bytes_read, nullptr))
    {
        return -1;
    }

    return static_cast<ssize_t>(bytes_read);
}

ssize_t ConPty::write(const void *data, size_t size)
{
    if (_pipe_in_write == INVALID_HANDLE_VALUE)
        return -1;

    DWORD bytes_written = 0;
    if (!WriteFile(_pipe_in_write, data, static_cast<DWORD>(size), &bytes_written, nullptr))
    {
        return -1;
    }

    return static_cast<ssize_t>(bytes_written);
}

bool ConPty::resize(const PtySize & size)
{
    if (!_hpc || !pfnResizePseudoConsole)
        return false;

    _size = size;

    COORD coord = {static_cast<SHORT>(size.cols), static_cast<SHORT>(size.rows)};
    HRESULT hr = pfnResizePseudoConsole(_hpc, coord);

    return SUCCEEDED(hr);
}

void ConPty::send_eof()
{
    if (_pipe_in_write != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_pipe_in_write);
        _pipe_in_write = INVALID_HANDLE_VALUE;
    }
}

// ============================================================================
// Factory functions for Windows
// ============================================================================

bool Pty::is_supported()
{
    return CheckConPtySupport();
}

std::unique_ptr<Pty> Pty::create()
{
    if (!CheckConPtySupport())
    {
        return nullptr;
    }
    return std::make_unique<ConPty>();
}

} // namespace sihd::ssh

#endif // SIHD_PTY_WINDOWS
