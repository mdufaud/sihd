#include <array>
#include <mutex>
#include <stdexcept>

#include <sihd/sys/platform.hpp>
#include <sihd/util/str.hpp>

// order is mandatory, cannot let clang-format mess with it

// clang-format off
# include <debugapi.h>
# include <psapi.h>
# include <winsock2.h>

# include <winsock.h>
# include <ws2def.h>

# include <dbghelp.h>
# include <winternl.h>
# include <windows.h>
# include <io.h>
// clang-format on

using sighandler_t = void (*)(int);

// same fallback as the one used by linux/os.cpp when rlimits are unavailable
#if !defined(__SIHD_UTIL_OS_DEFAULT_MAX_FDS__)
# define __SIHD_UTIL_OS_DEFAULT_MAX_FDS__ 512
#endif

using _NtQuerySystemInformation = NTSTATUS(WINAPI *)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

#include <ctype.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>

#include "internal/nt_dll.hpp"

namespace sihd::sys::os
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::os");

struct Wsa
{
        Wsa()
        {
            WORD wVersionRequested;
            WSADATA wsaData;

            /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
            wVersionRequested = MAKEWORD(2, 2);
            const int err = WSAStartup(wVersionRequested, &wsaData);
            if (err != 0)
            {
                SIHD_LOG(error, "WSAStartup failed: {} ({})", error_str(err), err);
            }
            else if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
            {
                SIHD_LOG(error, "Could not find a usable version of Winsock.dll");
            }
        }

        ~Wsa() { WSACleanup(); }
};

Wsa wsa;

namespace
{

struct SYSTEM_TIMEOFDAY_INFORMATION
{
        LARGE_INTEGER BootTime;
        LARGE_INTEGER CurrentTime;
        LARGE_INTEGER TimeZoneBias;
        ULONG TimeZoneId;
        ULONG Reserved;
        ULONGLONG BootTimeBias;
        ULONGLONG SleepTimeBias;
};

} // namespace

pid_t pid()
{
    return GetCurrentProcessId();
}

Result<rlim_t> max_fds()
{
    return __SIHD_UTIL_OS_DEFAULT_MAX_FDS__;
}

Result<void> ioctl(int fd, unsigned long request, void *arg_ptr)
{
    if (::ioctlsocket(fd, request, reinterpret_cast<long unsigned int *>(arg_ptr)) != 0)
        return std::unexpected(Error(ErrorCode::io_error, "could not ioctl: {}", last_socket_error_str()));
    return {};
}

Result<void> setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "cannot setsockopt on a negative socket"));
    if (::setsockopt(socket, level, optname, static_cast<const char *>(optval), optlen) != 0)
        return std::unexpected(Error(ErrorCode::io_error, "could not setsockopt: {}", last_socket_error_str()));
    return {};
}

Result<void> getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "cannot getsockopt on a negative socket"));
    if (::getsockopt(socket, level, optname, static_cast<char *>(optval), optlen) != 0)
        return std::unexpected(Error(ErrorCode::io_error, "could not getsockopt: {}", last_socket_error_str()));
    return {};
}

Result<FdType> fd_type(int fd)
{
    if (fd < 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "cannot get file type of a negative fd"));
    // GetFileType cannot tell a socket from a pipe (wine maps them alike): only sockets answer winsock SO_TYPE
    int so_type = 0;
    int so_len = sizeof(so_type);
    if (::getsockopt(fd, SOL_SOCKET, SO_TYPE, reinterpret_cast<char *>(&so_type), &so_len) == 0)
        return FdType::socket;
    const HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE)
        return std::unexpected(Error(ErrorCode::invalid_argument, "no handle behind fd {}", fd));
    switch (GetFileType(handle))
    {
        case FILE_TYPE_DISK:
            return FdType::regular;
        case FILE_TYPE_CHAR:
            return _isatty(fd) != 0 ? FdType::tty : FdType::unknown;
        case FILE_TYPE_PIPE:
            return FdType::pipe;
        default:
            return FdType::unknown;
    }
}

bool is_regular(int fd)
{
    return fd_type(fd) == FdType::regular;
}

bool is_tty(int fd)
{
    return _isatty(fd) != 0;
}

bool is_pipe(int fd)
{
    return fd_type(fd) == FdType::pipe;
}

bool is_socket(int fd)
{
    return fd_type(fd) == FdType::socket;
}

Result<void> set_nonblocking(int fd, bool enable)
{
    // FIONBIO goes through ioctlsocket, which drives sockets only on windows
    u_long mode = enable ? 1 : 0;
    return ioctl(fd, FIONBIO, &mode);
}

Result<size_t> bytes_available(int fd)
{
    u_long available = 0;
    if (ioctlsocket(fd, FIONREAD, &available) == 0)
        return static_cast<size_t>(available);
    const int socket_error = last_socket_error();
    // pipes only report their pending bytes through PeekNamedPipe
    const HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    DWORD pipe_available = 0;
    if (handle != INVALID_HANDLE_VALUE && PeekNamedPipe(handle, nullptr, 0, nullptr, &pipe_available, nullptr))
        return static_cast<size_t>(pipe_available);
    if (handle != INVALID_HANDLE_VALUE)
        return std::unexpected(Error(ErrorCode::io_error, "could not read available bytes: {}", last_error_str()));
    return std::unexpected(Error(ErrorCode::io_error, "could not read available bytes: {}", error_str(socket_error)));
}

Result<int> dup(int fd)
{
    const int new_fd = _dup(fd);
    if (new_fd == -1)
        return std::unexpected(Error::from_errno("could not dup"));
    return new_fd;
}

Result<int> dup2(int fd, int to)
{
    if (_dup2(fd, to) == -1)
        return std::unexpected(Error::from_errno("could not dup2"));
    return to;
}

Timestamp filetime_to_timestamp(uint64_t filetime_ticks)
{
    // 100ns ticks since 1601-01-01 -> nanoseconds since the unix epoch
    return Timestamp(static_cast<int64_t>((filetime_ticks - 116444736000000000ULL) * 100));
}

Duration filetime_to_duration(uint64_t filetime_ticks)
{
    return Duration(static_cast<int64_t>(filetime_ticks) * 100);
}

Timestamp filetime_now()
{
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return filetime_to_timestamp(uli.QuadPart);
}

Timestamp boot_time()
{
    static Timestamp boot_timestamp = Timestamp(0);
    if (boot_timestamp == 0)
    {
        SYSTEM_TIMEOFDAY_INFORMATION sysInfo;

        _NtQuerySystemInformation fct = reinterpret_cast<_NtQuerySystemInformation>(
            internal::nt_dll_symbol("ntdll", "NtQuerySystemInformation"));
        if (fct == nullptr)
            return Timestamp {};
        NTSTATUS status = fct(SystemTimeOfDayInformation, &sysInfo, sizeof(sysInfo), NULL);
        if (status != 0)
        {
            return Timestamp {};
        }

        boot_timestamp = filetime_to_timestamp(sysInfo.BootTime.QuadPart);
    }
    return boot_timestamp;
}

#ifndef SIHD_MAX_BACKTRACE_SIZE
# define SIHD_MAX_BACKTRACE_SIZE 50
#endif

#ifndef SIHD_DEFAULT_BACKTRACE_SIZE
# define SIHD_DEFAULT_BACKTRACE_SIZE 15
#endif

/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 */

Result<ssize_t> peak_rss()
{
    PROCESS_MEMORY_COUNTERS info;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)) == FALSE)
        return std::unexpected(Error(ErrorCode::io_error, "could not peak rss: {}", last_error_str()));
    return static_cast<ssize_t>(info.PeakWorkingSetSize);
}

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
ssize_t current_rss()
{
    PROCESS_MEMORY_COUNTERS info = {};
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return static_cast<ssize_t>(info.WorkingSetSize);
}

std::string error_str(int error_code)
{
    if (error_code == 0)
        return {};

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        static_cast<DWORD>(error_code),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        NULL);

    std::string message;
    if (messageBuffer != nullptr)
        message.assign(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

int last_error()
{
    return GetLastError();
}

std::string last_error_str()
{
    return error_str(GetLastError());
}

Result<void> mem_lock(const void *addr, size_t len)
{
    if (!VirtualLock(const_cast<LPVOID>(addr), len))
        return std::unexpected(Error(ErrorCode::io_error, "could not mem_lock: {}", last_error_str()));
    return {};
}

Result<void> mem_unlock(const void *addr, size_t len)
{
    if (!VirtualUnlock(const_cast<LPVOID>(addr), len))
        return std::unexpected(Error(ErrorCode::io_error, "could not mem_unlock: {}", last_error_str()));
    return {};
}

int last_socket_error()
{
    return WSAGetLastError();
}

std::string last_socket_error_str()
{
    return error_str(WSAGetLastError());
}

bool is_run_by_debugger()
{
    return IsDebuggerPresent();
}

bool is_run_by_wine()
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    return ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

ssize_t backtrace(int fd, size_t backtrace_size)
{
    constexpr size_t buffer_size = SIHD_MAX_BACKTRACE_SIZE;
    std::array<void *, buffer_size> buffer;
    static std::mutex dbghelp_mutex;

    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    if (handle == nullptr)
        return -1;

    unsigned int i;
    unsigned short frames;
    SYMBOL_INFO *symbol;
    HANDLE process;

    process = GetCurrentProcess();

    // DbgHelp is single-threaded
    std::lock_guard lock(dbghelp_mutex);
    SymInitialize(process, NULL, TRUE);

    const size_t wanted_size = std::min(backtrace_size, buffer_size);
    frames = CaptureStackBackTrace(0, wanted_size, buffer.data(), NULL);
    if (frames == 0)
        return -1;
    symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    if (symbol == nullptr)
        return -1;
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    std::string str = fmt::format("backtrace ({} calls)\n", frames);
    WriteFile(handle, str.c_str(), str.size(), NULL, NULL);
    for (i = 0; i < frames; i++)
    {
        if (SymFromAddr(process, (DWORD64)(buffer[i]), 0, symbol))
            str = fmt::sprintf("[%i] %s [0x%0llX]\n", frames - i - 1, symbol->Name, symbol->Address);
        else
            str = fmt::sprintf("[%i] ??? []\n", frames - i - 1);
        WriteFile(handle, str.c_str(), str.size(), NULL, NULL);
    }
    free(symbol);
    return (ssize_t)frames;
}

} // namespace sihd::sys::os
