#include <cstring>
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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
#include <sihd/util/Result.hpp>
#include <sihd/util/str.hpp>

#if !defined(__SIHD_UTIL_OS_DEFAULT_MAX_FDS__)
# define __SIHD_UTIL_OS_DEFAULT_MAX_FDS__ 512
#endif

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

namespace
{

// glibc's _GNU_SOURCE strerror_r returns char *, musl and emscripten return int
[[maybe_unused]] const char *strerror_result(int ret, const char *buf)
{
    return ret == 0 ? buf : "";
}

[[maybe_unused]] const char *strerror_result(const char *ret, const char *buf)
{
    return ret != nullptr ? ret : buf;
}

} // namespace

pid_t pid()
{
    return getpid();
}

Result<rlim_t> max_fds()
{
    if constexpr (build::is_emscripten)
    {
        return __SIHD_UTIL_OS_DEFAULT_MAX_FDS__;
    }
    struct rlimit r;
    if (getrlimit(RLIMIT_NOFILE, &r) == -1)
    {
        return std::unexpected(Error::from_errno("could not getrlimit max fds"));
    }
    return r.rlim_cur;
}

Result<void> ioctl(int fd, unsigned long request, void *arg_ptr)
{
    if (::ioctl(fd, request, arg_ptr) == -1)
        return std::unexpected(Error::from_errno("could not ioctl"));
    return {};
}

Result<void> setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "cannot setsockopt on a negative socket"));
    if (::setsockopt(socket, level, optname, optval, optlen) == -1)
        return std::unexpected(Error::from_errno("could not setsockopt"));
    return {};
}

Result<void> getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen)
{
    if (socket < 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "cannot getsockopt on a negative socket"));
    if (::getsockopt(socket, level, optname, optval, optlen) == -1)
        return std::unexpected(Error::from_errno("could not getsockopt"));
    return {};
}

Result<FdType> fd_type(int fd)
{
    if (fd < 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "cannot fstat a negative fd"));
    struct stat st;
    if (::fstat(fd, &st) == -1)
        return std::unexpected(Error::from_errno("could not fstat"));
    if (S_ISREG(st.st_mode))
        return FdType::regular;
    if (S_ISFIFO(st.st_mode))
        return FdType::pipe;
    if (S_ISSOCK(st.st_mode))
        return FdType::socket;
    if (S_ISCHR(st.st_mode) && ::isatty(fd))
        return FdType::tty;
    return FdType::unknown;
}

bool is_regular(int fd)
{
    return fd_type(fd) == FdType::regular;
}

bool is_tty(int fd)
{
    return ::isatty(fd) != 0;
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
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags == -1)
        return std::unexpected(Error::from_errno("could not get fd flags"));
    const int wanted = enable ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
    if (::fcntl(fd, F_SETFL, wanted) == -1)
        return std::unexpected(Error::from_errno("could not set fd flags"));
    return {};
}

Result<size_t> bytes_available(int fd)
{
    int available = 0;
    auto res = ioctl(fd, FIONREAD, &available);
    SIHD_UNEXPECTED_RETURN(res);
    return static_cast<size_t>(available);
}

Result<int> dup(int fd)
{
    const int new_fd = ::dup(fd);
    if (new_fd == -1)
        return std::unexpected(Error::from_errno("could not dup"));
    return new_fd;
}

Result<int> dup2(int fd, int to)
{
    if (::dup2(fd, to) == -1)
        return std::unexpected(Error::from_errno("could not dup2"));
    return to;
}

Timestamp boot_time()
{
    static Timestamp boot_timestamp = Timestamp(0);
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
    char chr;
    if (number < 10)
    {
        chr = (char)number + '0';
        return ::write(fd, &chr, 1);
    }
    ssize_t ret = write_number(fd, number / 10);
    chr = (char)(number % 10) + '0';
    return ret + ::write(fd, &chr, 1);
}

#endif

} // namespace

#if defined(SIHD_UTIL_OS_HAVE_BACKTRACE)

ssize_t backtrace(int fd, size_t backtrace_size)
{
    constexpr size_t buffer_size = SIHD_MAX_BACKTRACE_SIZE;
    void *buffer[buffer_size];
    const size_t wanted_size = std::min(backtrace_size, buffer_size);
    const size_t size = ::backtrace(buffer, wanted_size);
    bool ret = write(fd, "backtrace (") > 0;
    ret = ret && write_number(fd, size) > 0;
    ret = ret && write_endl(fd, " calls)") > 0;

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
        if (ret)
            backtrace_symbols_fd(buffer + i, 1, fd);
        ++i;
    }
    return size;
}

#else

# pragma message("Backtrace is not supported for this platform")

ssize_t backtrace(int fd, size_t backtrace_size)
{
    (void)fd;
    (void)backtrace_size;
    return 0;
}

#endif

/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 */

/**
 * Returns the peak (maximum so far) resident set size (physical
 * memory use) measured in bytes.
 */
Result<ssize_t> peak_rss()
{
    if constexpr (build::is_emscripten)
    {
        return std::unexpected(Error(ErrorCode::not_supported, "peak_rss is not supported on this platform"));
    }
#if defined(__SIHD_AIX__) || defined(__SIHD_SUN__)

    struct psinfo psinfo;
    int fd = -1;
    if ((fd = open("/proc/self/psinfo", O_RDONLY)) == -1)
    {
        return std::unexpected(Error::from_errno("could not open psinfo"));
    }
    if (read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo))
    {
        auto error = Error::from_errno("could not read psinfo");
        close(fd);
        return std::unexpected(std::move(error));
    }
    close(fd);
    return static_cast<ssize_t>(psinfo.pr_rssize * 1024L);

#elif defined(__SIHD_LINUX__) || defined(__SIHD_APPLE__)

    struct rusage rusage;
    if (getrusage(RUSAGE_SELF, &rusage) == -1)
    {
        return std::unexpected(Error::from_errno("could not getrusage"));
    }
# if defined(__SIHD_APPLE__)
    return static_cast<ssize_t>(rusage.ru_maxrss);
# else
    return static_cast<ssize_t>(rusage.ru_maxrss * 1024L);
# endif

#else
# pragma message("os::peak_rss is not supported on this platform")
    return std::unexpected(Error(ErrorCode::not_supported, "peak_rss is not supported on this platform"));
#endif
}

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
ssize_t current_rss()
{
    if constexpr (build::is_emscripten)
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
    char buf[128] = {};
    return strerror_result(strerror_r(error_code, buf, sizeof(buf)), buf);
}

Result<void> mem_lock(const void *addr, size_t len)
{
#if defined(__SIHD_EMSCRIPTEN__)
    (void)addr;
    (void)len;
    return std::unexpected(Error(ErrorCode::not_supported, "mem_lock is not supported on this platform"));
#else
    if (mlock(addr, len) == -1)
        return std::unexpected(Error::from_errno("could not mem_lock"));
    return {};
#endif
}

Result<void> mem_unlock(const void *addr, size_t len)
{
#if defined(__SIHD_EMSCRIPTEN__)
    (void)addr;
    (void)len;
    return std::unexpected(Error(ErrorCode::not_supported, "mem_unlock is not supported on this platform"));
#else
    if (munlock(addr, len) == -1)
        return std::unexpected(Error::from_errno("could not mem_unlock"));
    return {};
#endif
}

int last_error()
{
    return errno;
}

std::string last_error_str()
{
    return error_str(errno);
}

int last_socket_error()
{
    return errno;
}

std::string last_socket_error_str()
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

bool is_run_by_wine()
{
    return false;
}

} // namespace sihd::sys::os
