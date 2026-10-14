#include <io.h>
#include <windows.h>

#include <cstdio>
#include <memory>

#include <sihd/sys/console.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/Logger.hpp>

// mingw SDKs can predate ENABLE_VIRTUAL_TERMINAL_PROCESSING (windows 10)
#if !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
# define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

using namespace sihd::util;

namespace sihd::sys::console
{

SIHD_NEW_LOGGER("sihd::sys::console");

namespace
{

HANDLE handle_from_fd(int fd)
{
    return reinterpret_cast<HANDLE>(_get_osfhandle(fd));
}

} // namespace

sihd::util::Result<WindowSize> size(int fd)
{
    const HANDLE handle = handle_from_fd(fd);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (handle == INVALID_HANDLE_VALUE || GetConsoleScreenBufferInfo(handle, &info) == FALSE)
        return std::unexpected(
            Error(ErrorCode::not_supported, "could not get console window size: {}", os::last_error_str()));
    return WindowSize {info.srWindow.Right - info.srWindow.Left + 1, info.srWindow.Bottom - info.srWindow.Top + 1};
}

bool is_interactive()
{
    return _isatty(_fileno(stdin)) != 0;
}

void set_output_utf8()
{
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IOFBF, 1000);
}

struct RawMode::Impl
{
        int fd = -1;
        DWORD saved_mode = 0;
        bool applied = false;
};

RawMode::RawMode(int fd): _impl(std::make_unique<Impl>())
{
    const HANDLE handle = handle_from_fd(fd);
    if (handle == INVALID_HANDLE_VALUE || GetConsoleMode(handle, &_impl->saved_mode) == FALSE)
        return;
    const DWORD mode = _impl->saved_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    _impl->fd = fd;
    _impl->applied = SetConsoleMode(handle, mode) != FALSE;
}

RawMode::~RawMode()
{
    if (_impl->applied)
        SetConsoleMode(handle_from_fd(_impl->fd), _impl->saved_mode);
}

bool RawMode::valid() const
{
    return _impl->applied;
}

struct NoEcho::Impl
{
        int fd = -1;
        DWORD saved_mode = 0;
        bool applied = false;
};

NoEcho::NoEcho(int fd): _impl(std::make_unique<Impl>())
{
    const HANDLE handle = handle_from_fd(fd);
    if (handle == INVALID_HANDLE_VALUE || GetConsoleMode(handle, &_impl->saved_mode) == FALSE)
        return;
    const DWORD mode = _impl->saved_mode & ~ENABLE_ECHO_INPUT;
    _impl->fd = fd;
    _impl->applied = SetConsoleMode(handle, mode) != FALSE;
}

NoEcho::~NoEcho()
{
    if (_impl->applied)
        SetConsoleMode(handle_from_fd(_impl->fd), _impl->saved_mode);
}

bool NoEcho::valid() const
{
    return _impl->applied;
}

sihd::util::Result<void> enable_vt(int fd)
{
    const HANDLE handle = handle_from_fd(fd);
    DWORD mode = 0;
    if (handle == INVALID_HANDLE_VALUE || GetConsoleMode(handle, &mode) == FALSE
        || SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) == FALSE)
        return std::unexpected(
            Error(ErrorCode::not_supported, "could not enable virtual terminal: {}", os::last_error_str()));
    return {};
}

} // namespace sihd::sys::console
