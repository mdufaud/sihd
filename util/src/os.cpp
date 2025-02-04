#include <stdexcept>

#include <sihd/util/platform.hpp>
#include <sihd/util/str.hpp>

// for get_max_rss / peak_rss
#if defined(__SIHD_WINDOWS__)

# include <debugapi.h>
# include <psapi.h>
# include <winsock2.h>

# include <winsock.h>
# include <ws2def.h>

# include <dbghelp.h> // backtrace

# include <winternl.h>

# include <windows.h>

#elif defined(__SIHD_APPLE__)

# include <mach/mach.h>

#elif defined(__SIHD_AIX__) || defined(__SIHD_SUN__)

# include <fcntl.h>
# include <procfs.h>

#endif

#if defined(__SIHD_LINUX__)

# include <dlfcn.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/time.h>
# include <sys/wait.h>
# include <unistd.h>

# if !defined(__SIHD_EMSCRIPTEN__)
#  include <sys/ptrace.h>
# endif

#endif

// backtrace not available in windows / android / emscripten
#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <execinfo.h>
#endif

// for usage of sighandler_t in windows
#if defined(__SIHD_WINDOWS__)
typedef void (*sighandler_t)(int);
#endif

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Splitter.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>

#if !defined(__SIHD_UTIL_OS_DEFAULT_MAX_FDS__)
// backup for max_fds
# define __SIHD_UTIL_OS_DEFAULT_MAX_FDS__ 512
#endif

namespace sihd::util::os
{

SIHD_NEW_LOGGER("sihd::util::os");

std::string last_error_str()
{
#if !defined(__SIHD_WINDOWS__)
    return strerror(errno);
#else
    DWORD errorMessageID = GetLastError();
    if (errorMessageID == 0)
    {
        return std::string(); // No error
    }

    LPSTR messageBuffer = nullptr;
    size_t size
        = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                         NULL,
                         errorMessageID,
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                         (LPSTR)&messageBuffer,
                         0,
                         NULL);

    std::string message(messageBuffer, size);

    // Free the buffer allocated by FormatMessage
    LocalFree(messageBuffer);

    return message;
#endif
}

pid_t pid()
{
#if !defined(__SIHD_WINDOWS__)
    return getpid();
#else
    return GetCurrentProcessId();
#endif
}

rlim_t max_fds()
{
    if constexpr (os::is_emscripten)
    {
        return __SIHD_UTIL_OS_DEFAULT_MAX_FDS__;
    }
#if !defined(__SIHD_WINDOWS__)
    struct rlimit r;
    if (getrlimit(RLIMIT_NOFILE, &r) == -1)
    {
        SIHD_LOG(error, "OS: getrlim_t {}", strerror(errno));
        return 0;
    }
    return r.rlim_cur;
#else
    return __SIHD_UTIL_OS_DEFAULT_MAX_FDS__;
#endif
}

bool ioctl(int fd, unsigned long request, void *arg_ptr, bool logerror)
{
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::ioctl(fd, request, arg_ptr) == 0;
#else
    bool ret = ::ioctlsocket(fd, request, reinterpret_cast<long unsigned int *>(arg_ptr)) == 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: ioctl error: {}", strerror(errno));
    return ret == 0;
}

bool setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot setsockopt on a negative socket");
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::setsockopt(socket, level, optname, optval, optlen) >= 0;
#else
    bool ret = ::setsockopt(socket, level, optname, (const char *)optval, optlen) >= 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: {}", strerror(errno));
    return ret;
}

bool getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot getsockopt on a negative socket");
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::getsockopt(socket, level, optname, optval, optlen) >= 0;
#else
    bool ret = ::getsockopt(socket, level, optname, (char *)optval, optlen) >= 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: {}", strerror(errno));
    return ret;
}

Timestamp boot_time()
{
    static Timestamp boot_timestamp = 0;
    if (boot_timestamp == 0)
    {
#if defined(__SIHD_WINDOWS__)
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
        NTSTATUS status = NtQuerySystemInformation(SystemTimeOfDayInformation, &sysInfo, sizeof(sysInfo), NULL);
        if (status != 0)
        {
            return Timestamp {};
        }

        ULONGLONG epoch_time = (sysInfo.BootTime.QuadPart - 116444736000000000ULL) * 100;
        boot_timestamp = Timestamp(epoch_time);
#else
        auto content_opt = fs::read_all("/proc/stat");
        if (content_opt.has_value())
        {
            std::string & content = *content_opt;
            auto pos = content.find("btime");
            if (pos != content.npos)
            {
                std::string_view btime_line(content.data() + pos, content.size() - pos);
                btime_line.remove_prefix(btime_line.find_first_of(' ') + 1);
                btime_line = btime_line.substr(0, btime_line.find('\n'));
                time_t btime;
                if (str::to_long(btime_line, &btime))
                {
                    boot_timestamp = Timestamp(std::chrono::seconds(btime));
                }
            }
        }
#endif
    }
    return boot_timestamp;
}

bool is_root()
{
#if defined(__SIHD_WINDOWS__)
    HANDLE hToken = NULL;
    // Open the process token.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        return false;
    }

    // Retrieve the token elevation information.
    TOKEN_ELEVATION elevation;
    DWORD dwSize;
    if (!GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize))
    {
        CloseHandle(hToken);
        return false;
    }

    return elevation.TokenIsElevated;
#else
    return getuid() == 0;
#endif
}

// backtrace not available in windows / android

#ifndef SIHD_MAX_BACKTRACE_SIZE
# define SIHD_MAX_BACKTRACE_SIZE 50
#endif

#ifndef SIHD_DEFAULT_BACKTRACE_SIZE
# define SIHD_DEFAULT_BACKTRACE_SIZE 15
#endif

namespace
{

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

ssize_t write(int fd, const char *s)
{
    int i = 0;
    while (s[i])
        ++i;
    return ::write(fd, s, i);
}

ssize_t write_endl(int fd, const char *s)
{
    ssize_t ret = write(fd, s);
    return ret + ::write(fd, "\n", 1);
}

ssize_t write_number(int fd, int number)
{
    char c;
    if (number < 10)
    {
        c = (char)number + '0';
        return ::write(fd, &c, 1);
    }
    ssize_t ret = write_number(fd, number / 10);
    c = (char)(number % 10) + '0';
    return ret + ::write(fd, &c, 1);
}

#endif

} // namespace

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

ssize_t backtrace(int fd, size_t backtrace_size)
{
    constexpr size_t buffer_size = SIHD_MAX_BACKTRACE_SIZE;
    static void *buffer[buffer_size];
    static std::mutex buffer_mutex;

    std::lock_guard l(buffer_mutex);
    const size_t wanted_size = std::min(backtrace_size, buffer_size);
    const size_t size = ::backtrace(buffer, wanted_size);
    char **strings = (char **)backtrace_symbols(buffer, size);
    bool ret = write(fd, "backtrace (") > 0;
    ret = ret && write_number(fd, size) > 0;
    ret = ret && write_endl(fd, " calls)") > 0;
    if (strings == nullptr)
    {
        write_endl(fd, "Error while getting backtrace symbols");
        return -1;
    }
    uint32_t i = 0;
    while (ret && i < size)
    {
        ret = ret && write(fd, "[") > 0;
        ret = ret && write_number(fd, i) > 0;
        // no allocation allowed and less write if possible
        if (size >= 100)
        {
            if (i < 10)
                ret = ret && write(fd, "]   ") > 0;
            else if (i < 100)
                ret = ret && write(fd, "]  ") > 0;
            else
                ret = ret && write(fd, "] ") > 0;
        }
        else if (size >= 10)
        {
            if (i < 10)
                ret = ret && write(fd, "]  ") > 0;
            else
                ret = ret && write(fd, "] ") > 0;
        }
        else
        {
            ret = ret && write(fd, "] ") > 0;
        }
        ret = ret && write_endl(fd, strings[i]) > 0;
        ++i;
    }
    free(strings);
    return size;
}

#elif defined(__SIHD_WINDOWS__)

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

#else // no backtrace

# pragma message("Backtrace is not supported for this platform")

ssize_t backtrace(int fd, size_t backtrace_size)
{
    (void)fd;
    (void)backtrace_size;
    return 0;
}

#endif // end of backtrace

// debuggers

bool is_run_by_valgrind()
{
    char *ldpreload = getenv("LD_PRELOAD");
    return ldpreload != nullptr
           && (strstr(ldpreload, "/valgrind/") != nullptr || strstr(ldpreload, "/vgpreload") != nullptr);
}

bool is_run_by_debugger()
{
#if defined(__SIHD_EMSCRIPTEN__)
    return false;
#elif !defined(__SIHD_WINDOWS__)
    // https://stackoverflow.com/questions/3596781/how-to-detect-if-the-current-process-is-being-run-by-gdb
    char buf[4096];

    const int status_fd = open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
        return false;

    const ssize_t num_read = read(status_fd, buf, sizeof(buf) - 1);
    close(status_fd);

    if (num_read <= 0)
        return false;

    buf[num_read] = '\0';
    constexpr char tracerPidString[] = "TracerPid:";
    const auto tracer_pid_ptr = strstr(buf, tracerPidString);
    if (!tracer_pid_ptr)
        return false;

    for (const char *characterPtr = tracer_pid_ptr + sizeof(tracerPidString) - 1; characterPtr <= buf + num_read;
         ++characterPtr)
    {
        if (isspace(*characterPtr))
            continue;
        else
            return isdigit(*characterPtr) != 0 && *characterPtr != '0';
    }

    return false;
#else
    return IsDebuggerPresent();
#endif
}

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
    if constexpr (os::is_emscripten)
    {
        return -1L;
    }
#if defined(__SIHD_WINDOWS__)

    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (ssize_t)info.PeakWorkingSetSize;

#elif defined(__SIHD_AIX__) || defined(__SIHD_SUN__)

    struct psinfo psinfo;
    int fd = -1;
    if ((fd = open("/proc/self/psinfo", O_RDONLY)) == -1)
    {
        SIHD_LOG(error, "OS: peak_rss open: {}", strerror(errno));
        return (ssize_t)-1L;
    }
    if (read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo))
    {
        SIHD_LOG(error, "OS: peak_rss read: {}", strerror(errno));
        close(fd);
        return (ssize_t)-1L;
    }
    close(fd);
    return (ssize_t)(psinfo.pr_rssize * 1024L);

#elif defined(__SIHD_LINUX__) || defined(__SIHD_APPLE__)

    struct rusage rusage;
    if (getrusage(RUSAGE_SELF, &rusage) == -1)
    {
        SIHD_LOG(error, "OS: peak_rss getrusage: {}", strerror(errno));
        return (ssize_t)-1L;
    }
# if defined(__SIHD_APPLE__)
    return (ssize_t)rusage.ru_maxrss;
# else
    return (ssize_t)(rusage.ru_maxrss * 1024L);
# endif

#else
# pragma message("os::peak_rss is not supported on this platform")
    return (ssize_t)-1L;
#endif
}

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
ssize_t current_rss()
{
    if constexpr (os::is_emscripten)
    {
        return -1L;
    }
#if defined(__SIHD_WINDOWS__)

    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (ssize_t)info.WorkingSetSize;

#elif defined(__SIHD_APPLE__)

    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) != KERN_SUCCESS)
        return (ssize_t)-1L;
    return (ssize_t)info.resident_size;

#elif defined(__SIHD_LINUX__)

    long rss = 0L;
    FILE *fp = NULL;
    if ((fp = fopen("/proc/self/statm", "r")) == NULL)
        return (ssize_t)-1L;
    if (fscanf(fp, "%*s%ld", &rss) != 1)
    {
        fclose(fp);
        return (ssize_t)-1L;
    }
    fclose(fp);
    return (ssize_t)rss * (ssize_t)sysconf(_SC_PAGESIZE);

#else
# pragma message("os::current_rss is not supported on this platform")
    return (ssize_t)-1L;
#endif
}

bool exists_in_path(std::string_view binary_name)
{
    const char *path = getenv("PATH");
    if (path == nullptr)
        return false;

    Splitter splitter(":");
    for (const std::string & subpath : splitter.split(path))
    {
        if (fs::is_executable(fs::combine(subpath, binary_name)))
            return true;
    }

    return false;
}

} // namespace sihd::util::os
