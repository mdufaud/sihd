/**
 * @file Pty.cpp
 * @brief Windows ConPTY (pseudo-terminal) implementation.
 *
 * ConPTY is available on Windows 10 version 1809 (build 17763) and later;
 * Pty::is_supported() checks it at runtime.
 */

#include <cstring>

#include <sihd/sys/Environment.hpp>
#include <sihd/sys/Pty.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "internal/command_line.hpp"
#include "internal/environment.hpp"
#include "internal/nt_dll.hpp"

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// old MinGW SDKs lack the ConPTY types (old _WIN32_WINNT): API loaded dynamically, only the handle is needed
#ifndef HPCON
using HPCON = void *;
#endif

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

        void set_shell(std::string_view shell) override;
        void set_args(std::vector<std::string> args) override;
        void set_env(const std::string & name, const std::string & value) override;
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
        std::string _shell;             ///< Shell executable
        std::vector<std::string> _args; ///< Shell arguments
        Environment _env;
        std::string _working_dir; ///< Working directory
        PtySize _size;            ///< Terminal size

        HPCON _hpc;                              ///< Pseudo console handle
        HANDLE _pipe_in_read;                    ///< Pipe: PTY reads from here (we write to _pipe_in_write)
        HANDLE _pipe_in_write;                   ///< Pipe: We write input here
        HANDLE _pipe_out_read;                   ///< Pipe: We read output from here
        HANDLE _pipe_out_write;                  ///< Pipe: PTY writes to here
        PROCESS_INFORMATION _pi;                 ///< Process and main thread handles
        LPPROC_THREAD_ATTRIBUTE_LIST _attr_list; ///< Thread attribute list for ConPTY
        bool _spawned;
        int _exit_status;
};

namespace
{

using PFN_CreatePseudoConsole = HRESULT(WINAPI *)(COORD, HANDLE, HANDLE, DWORD, HPCON *);
using PFN_ResizePseudoConsole = HRESULT(WINAPI *)(HPCON, COORD);
using PFN_ClosePseudoConsole = void(WINAPI *)(HPCON);

// loaded dynamically: absent before Windows 10 1809
struct ConPtyApi
{
        PFN_CreatePseudoConsole create = nullptr;
        PFN_ResizePseudoConsole resize = nullptr;
        PFN_ClosePseudoConsole close = nullptr;

        bool available() const { return create && resize && close; }
};

ConPtyApi load_conpty_api()
{
    ConPtyApi api;
    api.create = reinterpret_cast<PFN_CreatePseudoConsole>(internal::nt_dll_symbol("kernel32", "CreatePseudoConsole"));
    api.resize = reinterpret_cast<PFN_ResizePseudoConsole>(internal::nt_dll_symbol("kernel32", "ResizePseudoConsole"));
    api.close = reinterpret_cast<PFN_ClosePseudoConsole>(internal::nt_dll_symbol("kernel32", "ClosePseudoConsole"));

    if (api.available())
        SIHD_LOG(debug, "ConPty: API available (Windows 10 1809+)");
    else
        SIHD_LOG(debug, "ConPty: API not available (requires Windows 10 1809+)");
    return api;
}

const ConPtyApi & conpty_api()
{
    static const ConPtyApi api = load_conpty_api();
    return api;
}

} // namespace

ConPty::ConPty():
    _shell("cmd.exe"),
    _size {80, 24, 0, 0},
    _hpc(nullptr),
    _pipe_in_read(INVALID_HANDLE_VALUE),
    _pipe_in_write(INVALID_HANDLE_VALUE),
    _pipe_out_read(INVALID_HANDLE_VALUE),
    _pipe_out_write(INVALID_HANDLE_VALUE),
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

    if (_hpc && conpty_api().close)
    {
        conpty_api().close(_hpc);
        _hpc = nullptr;
    }

    if (_pipe_in_read != INVALID_HANDLE_VALUE)
        CloseHandle(_pipe_in_read);
    if (_pipe_in_write != INVALID_HANDLE_VALUE)
        CloseHandle(_pipe_in_write);
    if (_pipe_out_read != INVALID_HANDLE_VALUE)
        CloseHandle(_pipe_out_read);
    if (_pipe_out_write != INVALID_HANDLE_VALUE)
        CloseHandle(_pipe_out_write);

    if (_pi.hProcess != INVALID_HANDLE_VALUE && _pi.hProcess != nullptr)
        CloseHandle(_pi.hProcess);
    if (_pi.hThread != INVALID_HANDLE_VALUE && _pi.hThread != nullptr)
        CloseHandle(_pi.hThread);

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

void ConPty::set_env(const std::string & name, const std::string & value)
{
    _env.set(name, value);
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

    if (!conpty_api().available())
    {
        SIHD_LOG(error, "ConPty: not supported on this Windows version");
        return false;
    }

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

    COORD size = {static_cast<SHORT>(_size.cols), static_cast<SHORT>(_size.rows)};
    HRESULT hr = conpty_api().create(size, _pipe_in_read, _pipe_out_write, 0, &_hpc);
    if (FAILED(hr))
    {
        SIHD_LOG(error, "ConPty: CreatePseudoConsole failed: 0x{:08x}", static_cast<unsigned>(hr));
        return false;
    }

    // the ConPTY duplicates its pipe ends internally: close ours
    CloseHandle(_pipe_in_read);
    _pipe_in_read = INVALID_HANDLE_VALUE;
    CloseHandle(_pipe_out_write);
    _pipe_out_write = INVALID_HANDLE_VALUE;

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

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
# define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif
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

    std::vector<std::string_view> arguments {_shell};
    for (const auto & arg : _args)
        arguments.emplace_back(arg);
    const std::string cmdline = internal::command_line(arguments);
    Environment environment = Environment::from_current();
    for (const auto & [key, value] : _env.entries())
        environment.set(key, value);
    const std::string env_block = internal::to_windows_block(environment);

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
                        const_cast<char *>(env_block.data()),
                        working_dir,
                        &si.StartupInfo,
                        &_pi))
    {
        SIHD_LOG(error, "ConPty: CreateProcess failed: {}", os::last_error_str());
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

    // no graceful termination signal like SIGTERM on windows: terminate directly
    TerminateProcess(_pi.hProcess, 1);
}

int ConPty::read_fd() const
{
    // windows HANDLE is no select() fd: callers use WaitForSingleObject, we return a placeholder
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
        return 0;
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
    if (!_hpc || !conpty_api().resize)
        return false;

    _size = size;

    COORD coord = {static_cast<SHORT>(size.cols), static_cast<SHORT>(size.rows)};
    HRESULT hr = conpty_api().resize(_hpc, coord);

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

bool Pty::is_supported()
{
    return conpty_api().available();
}

std::unique_ptr<Pty> Pty::create()
{
    if (!conpty_api().available())
    {
        return nullptr;
    }
    return std::make_unique<ConPty>();
}

} // namespace sihd::sys
