#include <stdexcept>

#include <sihd/sys/platform.hpp>
#include <sihd/util/str.hpp>

#if defined(__SIHD_APPLE__)

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

// glibc backtrace; musl, android, emscripten and apple have none
#if defined(__GLIBC__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# define SIHD_UTIL_OS_HAVE_BACKTRACE
# include <execinfo.h>
#endif

#include <ctype.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

#if !defined(__SIHD_UTIL_OS_DEFAULT_MAX_FDS__)
// backup for max_fds
# define __SIHD_UTIL_OS_DEFAULT_MAX_FDS__ 512
#endif

// backtrace not available in android / emscripten / apple

#ifndef SIHD_MAX_BACKTRACE_SIZE
# define SIHD_MAX_BACKTRACE_SIZE 50
#endif

#ifndef SIHD_DEFAULT_BACKTRACE_SIZE
# define SIHD_DEFAULT_BACKTRACE_SIZE 15
#endif

namespace sihd::sys::os
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::os");

pid_t pid()
{
    return getpid();
}

rlim_t max_fds()
{
    if constexpr (sihd::util::build::is_emscripten)
    {
        return __SIHD_UTIL_OS_DEFAULT_MAX_FDS__;
    }
    struct rlimit r;
    if (getrlimit(RLIMIT_NOFILE, &r) == -1)
    {
        SIHD_LOG(error, "OS: getrlim_t {}", last_error_str());
        return 0;
    }
    return r.rlim_cur;
}

bool ioctl(int fd, unsigned long request, void *arg_ptr, bool logerror)
{
    bool ret = ::ioctl(fd, request, arg_ptr) == 0;
    if (!ret && logerror)
        SIHD_LOG(error, "OS: ioctl error: {}", last_error_str());
    return ret;
}

bool setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot setsockopt on a negative socket");
    bool ret = ::setsockopt(socket, level, optname, optval, optlen) >= 0;
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: {}", last_error_str());
    return ret;
}

bool getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot getsockopt on a negative socket");
    bool ret = ::getsockopt(socket, level, optname, optval, optlen) >= 0;
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: {}", last_error_str());
    return ret;
}

Timestamp boot_time()
{
    static Timestamp boot_timestamp = 0;
    if (boot_timestamp == 0)
    {
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
                if (const auto btime = str::convert_from_string<long long>(btime_line))
                {
                    boot_timestamp = Timestamp(std::chrono::seconds(*btime));
                }
            }
        }
    }
    return boot_timestamp;
}

namespace
{

#if defined(SIHD_UTIL_OS_HAVE_BACKTRACE)

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

#if defined(SIHD_UTIL_OS_HAVE_BACKTRACE)

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
    if constexpr (sihd::util::build::is_emscripten)
    {
        return -1L;
    }
#if defined(__SIHD_AIX__) || defined(__SIHD_SUN__)

    struct psinfo psinfo;
    int fd = -1;
    if ((fd = open("/proc/self/psinfo", O_RDONLY)) == -1)
    {
        SIHD_LOG(error, "OS: peak_rss open: {}", last_error_str());
        return (ssize_t)-1L;
    }
    if (read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo))
    {
        SIHD_LOG(error, "OS: peak_rss read: {}", last_error_str());
        close(fd);
        return (ssize_t)-1L;
    }
    close(fd);
    return (ssize_t)(psinfo.pr_rssize * 1024L);

#elif defined(__SIHD_LINUX__) || defined(__SIHD_APPLE__)

    struct rusage rusage;
    if (getrusage(RUSAGE_SELF, &rusage) == -1)
    {
        SIHD_LOG(error, "OS: peak_rss getrusage: {}", last_error_str());
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
    if constexpr (sihd::util::build::is_emscripten)
    {
        return -1L;
    }
#if defined(__SIHD_APPLE__)

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

std::string error_str(int error_code)
{
    return strerror(error_code);
}

std::string last_error_str()
{
    return error_str(errno);
}

bool is_run_by_debugger()
{
#if defined(__SIHD_EMSCRIPTEN__)
    return false;
#else
    char buf[4096];

    const int status_fd = ::open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
        return false;

    const ssize_t num_read = ::read(status_fd, buf, sizeof(buf) - 1);
    ::close(status_fd);

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
#endif
}

} // namespace sihd::sys::os
