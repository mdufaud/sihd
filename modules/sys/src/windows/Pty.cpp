/**
 * @file Pty.cpp
 * @brief Windows ConPTY (pseudo-terminal) implementation.
 *
 * ConPTY is available on Windows 10 version 1809 (build 17763) and later;
 * Pty::is_supported() checks it at runtime.
 */

#include <cstring>

#include <sihd/sys/Pty.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace sihd::sys
{

SIHD_LOGGER;

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

    pfnCreatePseudoConsole = reinterpret_cast<PFN_CreatePseudoConsole>(
        reinterpret_cast<void *>(GetProcAddress(kernel32, "CreatePseudoConsole")));
    pfnResizePseudoConsole = reinterpret_cast<PFN_ResizePseudoConsole>(
        reinterpret_cast<void *>(GetProcAddress(kernel32, "ResizePseudoConsole")));
    pfnClosePseudoConsole = reinterpret_cast<PFN_ClosePseudoConsole>(
        reinterpret_cast<void *>(GetProcAddress(kernel32, "ClosePseudoConsole")));

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

# ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#  define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
# endif
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

} // namespace sihd::sys
