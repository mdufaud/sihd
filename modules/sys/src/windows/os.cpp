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

# include <dbghelp.h> // backtrace
# include <winternl.h>
# include <windows.h>
# include <io.h>
// clang-format on

// for usage of sighandler_t in windows
typedef void (*sighandler_t)(int);

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
#include <sihd/util/Splitter.hpp>
#include <sihd/util/str.hpp>

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
            int err;

            /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
            wVersionRequested = MAKEWORD(2, 2);
            err = WSAStartup(wVersionRequested, &wsaData);
            if (err != 0)
            {
                SIHD_LOG(error, "WSAStartup failed: {} ({})", last_error_str(), err);
            }
            if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
            {
                SIHD_LOG(error, "Could not find a usable version of Winsock.dll");
            }
        }

        ~Wsa() { WSACleanup(); }
};

Wsa wsa;

pid_t pid()
{
    return GetCurrentProcessId();
}

rlim_t max_fds()
{
    return 512;
}

bool ioctl(int fd, unsigned long request, void *arg_ptr, bool logerror)
{
    bool ret = ::ioctlsocket(fd, request, reinterpret_cast<long unsigned int *>(arg_ptr)) == 0;
    if (!ret && logerror)
        SIHD_LOG(error, "OS: ioctl error: {}", last_error_str());
    return ret;
}

bool setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot setsockopt on a negative socket");
    bool ret = ::setsockopt(socket, level, optname, (const char *)optval, optlen) >= 0;
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: {}", last_error_str());
    return ret;
}

bool getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot getsockopt on a negative socket");
    bool ret = ::getsockopt(socket, level, optname, (char *)optval, optlen) >= 0;
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: {}", last_error_str());
    return ret;
}

Timestamp filetime_to_timestamp(uint64_t filetime_ticks)
{
    // 100ns ticks since 1601-01-01 -> nanoseconds since the unix epoch
    return Timestamp(static_cast<int64_t>((filetime_ticks - 116444736000000000ULL) * 100));
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
    static Timestamp boot_timestamp = 0;
    if (boot_timestamp == 0)
    {
        auto uptime = std::chrono::milliseconds(GetTickCount64());
        typedef struct _SYSTEM_TIMEOFDAY_INFORMATION
        {
                LARGE_INTEGER BootTime;
                LARGE_INTEGER CurrentTime;
                LARGE_INTEGER TimeZoneBias;
                ULONG TimeZoneId;
                ULONG Reserved;
                ULONGLONG BootTimeBias;
                ULONGLONG SleepTimeBias;
        } SYSTEM_TIMEOFDAY_INFORMATION;

        SYSTEM_TIMEOFDAY_INFORMATION sysInfo;

        void *ptr = (void *)GetProcAddress(GetModuleHandle("ntdll"), "NtQuerySystemInformation");
        if (ptr == nullptr)
            return Timestamp {};
        _NtQuerySystemInformation fct = reinterpret_cast<_NtQuerySystemInformation>(ptr);
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

// debuggers

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

/**
 * Returns the peak (maximum so far) resident set size (physical
 * memory use) measured in bytes, or zero if the value cannot be
 * determined on this OS.
 */
ssize_t peak_rss()
{
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (ssize_t)info.PeakWorkingSetSize;
}

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
ssize_t current_rss()
{
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (ssize_t)info.WorkingSetSize;
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

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

std::string last_error_str()
{
    return error_str(WSAGetLastError());
}

bool is_run_by_debugger()
{
    return IsDebuggerPresent();
}

ssize_t backtrace(int fd, size_t backtrace_size)
{
    constexpr size_t buffer_size = SIHD_MAX_BACKTRACE_SIZE;
    static void *buffer[buffer_size];
    static std::mutex buffer_mutex;

    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    if (handle == nullptr)
        return -1;

    unsigned int i;
    unsigned short frames;
    SYMBOL_INFO *symbol;
    HANDLE process;

    process = GetCurrentProcess();

    SymInitialize(process, NULL, TRUE);

    std::lock_guard l(buffer_mutex);
    const size_t wanted_size = std::min(backtrace_size, buffer_size);
    frames = CaptureStackBackTrace(0, wanted_size, buffer, NULL);
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
