#include <sys/stat.h>
#include <unistd.h>

#include <sihd/util/platform.hpp>

// for get_max_rss / peak_rss
#if defined(__SIHD_WINDOWS__)

# include <debugapi.h>
# include <direct.h> // _stat
# include <psapi.h>
# include <winsock2.h>

# include <winsock.h>
# include <ws2def.h>

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

bool stat(const char *pathname, struct stat *statbuf, bool logerror)
{
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::stat(pathname, statbuf) == 0;
#else
    bool ret = ::_stat(pathname, reinterpret_cast<struct _stat *>(statbuf)) == 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: stat error: {}", strerror(errno));
    return ret == 0;
}

bool fstat(int fd, struct stat *statbuf, bool logerror)
{
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::fstat(fd, statbuf) == 0;
#else
    bool ret = ::_fstat(fd, reinterpret_cast<struct _stat *>(statbuf)) == 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: fstat error: {}", strerror(errno));
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

bool is_root()
{
#if defined(__SIHD_WINDOWS__)
    BOOL is_admin = FALSE;
    DWORD dwError = ERROR_SUCCESS;
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

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

# ifndef SIHD_MAX_BACKTRACE_SIZE
#  define SIHD_MAX_BACKTRACE_SIZE 50
# endif

# ifndef SIHD_DEFAULT_BACKTRACE_SIZE
#  define SIHD_DEFAULT_BACKTRACE_SIZE 15
# endif

namespace
{

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

static void *backtrace_buffer[SIHD_MAX_BACKTRACE_SIZE];

} // namespace

ssize_t backtrace(int fd, size_t backtrace_size)
{
    size_t wanted_size = std::min(backtrace_size, (size_t)SIHD_MAX_BACKTRACE_SIZE);
    size_t size = ::backtrace(backtrace_buffer, wanted_size);
    char **strings = (char **)backtrace_symbols(backtrace_buffer, size);
    bool ret = write(fd, "backtrace (") > 0;
    ret = ret && write_number(fd, size) > 0;
    ret = ret && write_endl(fd, " calls)") > 0;
    if (strings == nullptr)
    {
        ret = ret && write_endl(fd, "Error while getting backtrace symbols");
        return -1;
    }
    uint32_t i = 0;
    while (ret && i < size)
    {
        ret = ret && write(fd, "[") > 0;
        ret = ret && write_number(fd, i) > 0;
        ret = ret && write(fd, "]\t--> ") > 0;
        ret = ret && write_endl(fd, strings[i]) > 0;
        ++i;
    }
    free(strings);
    return size;
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

// https://stackoverflow.com/questions/3596781/how-to-detect-if-the-current-process-is-being-run-by-gdb

bool is_run_by_debugger()
{
#if defined(__SIHD_EMSCRIPTEN__)
    return false;
#elif !defined(__SIHD_WINDOWS__)
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
