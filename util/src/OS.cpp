#include <sihd/util/OS.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/AtExit.hpp>
#include <sihd/util/FS.hpp>

#include <vector>
#include <algorithm>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

// for get_max_rss / peak_rss
#if defined(__SIHD_WINDOWS__)
# include <windows.h>
# include <psapi.h>
# include <debugapi.h>
# include <direct.h> // _stat
#elif defined(__SIHD_UNIX__) || defined(__SIHD_APPLE__)
# include <unistd.h>
# include <sys/resource.h>
# if defined(__SIHD_APPLE__)
#  include <mach/mach.h>
# endif
#elif defined(__SIHD_AIX__) || defined(__SIHD_SUN__)
# include <fcntl.h>
# include <procfs.h>
#endif

#if defined(__SIHD_LINUX__)
# include <sys/types.h>
# include <sys/ptrace.h>
# include <sys/wait.h>
# include <sys/time.h>
# include <fcntl.h>
#endif

// backtrace not available in windows / android
#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <execinfo.h>
#endif

// for usage of sighandler_t in windows
#if defined(__SIHD_WINDOWS__)
typedef void (*sighandler_t)(int);
#endif

#if !defined(__SIHD_UTIL_OS_DEFAULT_MAX_FDS__)
// backup for max_fds
# define __SIHD_UTIL_OS_DEFAULT_MAX_FDS__ 512
#endif

namespace sihd::util
{

SIHD_LOGGER;

bool OS::signal_used = false;
std::mutex OS::signal_mutex;
std::map<int, std::list<IHandler<int> *>> OS::map_signals_handlers;

bool    OS::clear_signal_handlers(int sig)
{
    std::lock_guard lock(OS::signal_mutex);
    for (IHandler<int> *handler : OS::map_signals_handlers[sig])
    {
        delete handler;
    }
    OS::map_signals_handlers[sig].clear();
    return unhandle_signal(sig);
}

bool    OS::clear_signal_handlers()
{
    std::lock_guard lock(OS::signal_mutex);
    bool ret = true;
    for (auto & [sig, handlers_lst] : OS::map_signals_handlers)
    {
        for (IHandler<int> *handler : handlers_lst)
        {
            delete handler;
        }
        handlers_lst.clear();
        if (unhandle_signal(sig) == false)
            ret = false;
    }
    OS::map_signals_handlers.clear();
    return ret;
}

bool    OS::clear_signal_handler(int sig, IHandler<int> *runnable)
{
    std::lock_guard lock(OS::signal_mutex);
    auto & lst = OS::map_signals_handlers[sig];
    auto it = std::find(lst.begin(), lst.end(), runnable);
    if (it != lst.end())
    {
        lst.erase(it);
        if (lst.empty())
            return unhandle_signal(sig);
        return true;
    }
    return false;
}

void    OS::_signal_callback(int sig)
{
    SIHD_LOG(debug, "Signal caught: " << OS::signal_name(sig));
    std::lock_guard lock(OS::signal_mutex);
    for (IHandler<int> *handler : OS::map_signals_handlers[sig])
    {
        handler->handle(sig);
    }
}

bool    OS::add_signal_handler(int sig, IHandler<int> *handler)
{
    sighandler_t sighandler = signal(sig, OS::_signal_callback);
    if (sighandler == SIG_ERR)
    {
        SIHD_LOG(error, "Error handling signal: " << OS::signal_name(sig));
        return false;
    }
    std::lock_guard lock(OS::signal_mutex);
    OS::map_signals_handlers[sig].push_back(handler);
    if (OS::signal_used == false)
    {
        AtExit::add_handler(new Runnable([] () -> bool
        {
            OS::clear_signal_handlers();
            return true;
        }));
        AtExit::install();
        OS::signal_used = true;
    }
    return true;
}

bool    OS::unhandle_signal(int sig)
{
    sighandler_t handler = signal(sig, SIG_DFL);
    if (handler == SIG_ERR)
    {
        SIHD_LOG(error, "Error removing signal: " << OS::signal_name(sig));
        return false;
    }
    return true;
}

std::string OS::signal_name(int sig)
{
#if !defined(__SIHD_WINDOWS__)
    char *signame = strsignal(sig);
    if (signame == nullptr)
        return std::to_string(sig);
    return signame;
#else
    return std::to_string(sig);
#endif
}

// utilities

bool   OS::kill(pid_t pid, int sig)
{
#if !defined(__SIHD_WINDOWS__)
    return ::kill(pid, sig) == 0;
#else
    (void)pid;
    (void)sig;
    return false;
#endif
}

pid_t   OS::pid()
{
#if !defined(__SIHD_WINDOWS__)
    return getpid();
#else
    return GetCurrentProcessId();
#endif
}

sihd_rlim_t OS::max_fds()
{
#if !defined(__SIHD_WINDOWS__)
    struct rlimit r;
    if (getrlimit(RLIMIT_NOFILE, &r) == -1)
    {
        SIHD_LOG(error, "OS: getrlim_t " << strerror(errno));
        return 0;
    }
    return r.rlim_cur;
#else
    return __SIHD_UTIL_OS_DEFAULT_MAX_FDS__;
#endif
}

bool    OS::ioctl(int fd, unsigned long request, void *arg_ptr, bool logerror)
{
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::ioctl(fd, request, arg_ptr) == 0;
#else
    bool ret = ::ioctlsocket(fd, request, reinterpret_cast<long unsigned int *>(arg_ptr)) == 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: ioctl error: " << strerror(errno));
    return ret == 0;
}

bool    OS::stat(const char *pathname, struct stat *statbuf, bool logerror)
{
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::stat(pathname, statbuf) == 0;
#else
    bool ret = ::_stat(pathname, reinterpret_cast<struct _stat64i32 *>(statbuf)) == 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: stat error: " << strerror(errno));
    return ret == 0;
}

bool    OS::fstat(int fd, struct stat *statbuf, bool logerror)
{
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::fstat(fd, statbuf) == 0;
#else
    bool ret = ::_fstat(fd, reinterpret_cast<struct _stat64i32 *>(statbuf)) == 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: fstat error: " << strerror(errno));
    return ret == 0;
}

bool    OS::setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot setsockopt on a negative socket");
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::setsockopt(socket, level, optname, optval, optlen) >= 0;
#else
    bool ret = ::setsockopt(socket, level, optname, (const char *)optval, optlen) >= 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: " << strerror(errno));
    return ret;
}

bool    OS::getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot getsockopt on a negative socket");
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::getsockopt(socket, level, optname, optval, optlen) >= 0;
#else
    bool ret = ::getsockopt(socket, level, optname, (char *)optval, optlen) >= 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: " << strerror(errno));
    return ret;
}

bool    OS::is_root()
{
#if defined(__SIHD_WINDOWS__)
    //TODO
    return false;
#else
    return getuid() == 0;
#endif
}

std::string OS::home_path()
{
#if defined(__SIHD_WINDOWS__)
    return FS::combine(getenv("HOMEDRIVE"), getenv("HOMEPATH"));
#else
    return getenv("HOME");
#endif
}

std::string OS::cwd()
{
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != nullptr)
        return std::string(cwd);
    return "";
}

std::string OS::executable_path()
{
#if defined(__SIHD_WINDOWS__)
    char path[MAX_PATH];
    if (GetModuleFileName(NULL, path, MAX_PATH) != 0)
        return path;
#else
    std::ifstream mapf("/proc/self/maps");
    std::string line;
    std::string path;
    if (std::getline(mapf, line))
    {
        size_t idx = line.find("/");
        if (idx != std::string::npos)
        {
            path = line.substr(idx);
            return path;
        }
    }
#endif
    return ".";
}

// backtrace not available in windows / android

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

# ifndef SIHD_MAX_BACKTRACE_SIZE
#  define SIHD_MAX_BACKTRACE_SIZE 100
# endif

# ifndef SIHD_DEFAULT_BACKTRACE_SIZE
#  define SIHD_DEFAULT_BACKTRACE_SIZE 15
# endif

void *OS::backtrace_buffer[SIHD_MAX_BACKTRACE_SIZE];
int OS::backtrace_size = SIHD_DEFAULT_BACKTRACE_SIZE;

ssize_t  OS::write(int fd, const char *s)
{
    int i = 0;
    while (s[i])
        ++i;
    return ::write(fd, s, i);
}

ssize_t  OS::write_endl(int fd, const char *s)
{
    ssize_t ret = write(fd, s);
    return ret + ::write(fd, "\n", 1);
}

ssize_t   OS::write_number(int fd, int number)
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

ssize_t    OS::backtrace(int fd)
{
    size_t wanted_size = std::min(OS::backtrace_size, SIHD_MAX_BACKTRACE_SIZE);
    size_t size = ::backtrace(OS::backtrace_buffer, wanted_size);
    char **strings = (char **)backtrace_symbols(OS::backtrace_buffer, size);
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

ssize_t    OS::backtrace(int fd)
{
    (void)fd;
    return -1;
}

#endif // end of backtrace

// debuggers

bool    OS::is_run_with_asan()
{
#if defined(__SANITIZE_ADDRESS__)
    return true;
#else
    return false;
#endif
}

bool    OS::is_run_by_valgrind()
{
    char *ldpreload = getenv("LD_PRELOAD");
    return ldpreload != nullptr
            && (strstr(ldpreload, "/valgrind/") != nullptr
                || strstr(ldpreload, "/vgpreload") != nullptr);
}

#if !defined(__SIHD_WINDOWS__)

bool    OS::is_run_by_debugger()
{
    // gdb check
    int pid = fork();
    int status;
    int res = -1;

    if (pid == -1)
    {
        SIHD_LOG(error, "OS: fork error: " << strerror(errno));
        return false;
    }
    if (pid == 0)
    {
        int ppid = getppid();
        /* Child */
        if (ptrace(PTRACE_ATTACH, ppid, NULL, NULL) == 0)
        {
            /* Wait for the parent to stop and continue it */
            waitpid(ppid, NULL, 0);
            ptrace(PTRACE_CONT, NULL, NULL);

            /* Detach */
            ptrace(PTRACE_DETACH, getppid(), NULL, NULL);

            /* We were the tracers, so gdb is not present */
            res = 0;
        }
        else
        {
            /* Trace failed so gdb is present */
            res = 1;
        }
        exit(res);
    }
    else
    {
        waitpid(pid, &status, 0);
        res = WEXITSTATUS(status);
    }
    return res == 1;
}

#else

bool    OS::is_run_by_debugger()
{
    return IsDebuggerPresent();
}

#endif

// dynlib

#if !defined(__SIHD_WINDOWS__)

void *OS::load_lib(const std::string & lib_name)
{
    std::vector<std::string> to_try = {
        "lib" + lib_name + ".so",
        lib_name + ".so",
        lib_name,
    };
    void *handle = nullptr;
    for (const std::string & lib : to_try)
    {
        handle = dlopen(lib.c_str(), RTLD_NOW);
        if (handle != nullptr)
            break ;
    }
    return handle;
}

void *OS::load_symbol(void *handle, const std::string & sym_name)
{
    if (handle == nullptr)
        return nullptr;
    return dlsym(handle, sym_name.c_str());
}

std::string OS::lib_error()
{
    return dlerror();
}

bool OS::close_lib(void *handle)
{
    if (handle == nullptr)
        return false;
    return dlclose(handle) == 0;
}

void *OS::load_symbol_unload_lib(const std::string & lib_name, const std::string & sym_name)
{
    void *handle = OS::load_lib(lib_name);
    if (handle == nullptr)
    {
        SIHD_LOG(error, "OS: could not load library: " << OS::lib_error());
        return nullptr;
    }
    void *sym_ptr = OS::load_symbol(handle, sym_name);
    if (sym_ptr == nullptr)
    {
        SIHD_LOG(error, "OS: could not load symbol: " << OS::lib_error());
        return nullptr;
    }
    if (dlclose(handle) != 0)
        SIHD_LOG(warning, "OS: could not close lib handle: " << OS::lib_error());
    return sym_ptr;
}

#else

void *OS::load_symbol_unload_lib(const std::string & lib_name, const std::string & sym_name)
{
    (void)lib_name;
    (void)sym_name;
    return nullptr;
}

#endif // end of shared lib

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
size_t  OS::peak_rss()
{
#if defined(__SIHD_WINDOWS__)

    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (size_t)info.PeakWorkingSetSize;

#elif defined(__SIHD_AIX__) || defined(__SIHD_SUN__)

    struct psinfo psinfo;
    int fd = -1;
    if ((fd = open("/proc/self/psinfo", O_RDONLY)) == -1)
    {
        SIHD_LOG(error, "OS: peak_rss open: " << strerror(errno));
        return (size_t)0L;
    }
    if (read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo))
    {
        SIHD_LOG(error, "OS: peak_rss read: " << strerror(errno));
        close(fd);
        return (size_t)0L;
    }
    close(fd);
    return (size_t)(psinfo.pr_rssize * 1024L);

#elif defined(__SIHD_LINUX__) || defined(__SIHD_APPLE__)

    struct rusage rusage;
    if (getrusage(RUSAGE_SELF, &rusage) == -1)
    {
        SIHD_LOG(error, "OS: peak_rss getrusage: " << strerror(errno));
        return (size_t)0L;
    }
# if defined(__SIHD_APPLE__)
    return (size_t)rusage.ru_maxrss;
# else
    return (size_t)(rusage.ru_maxrss * 1024L);
# endif

#else
    return (size_t)0L;
#endif
}

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
size_t  OS::current_rss()
{
#if defined(__SIHD_WINDOWS__)

    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (size_t)info.WorkingSetSize;

#elif defined(__SIHD_APPLE__)

    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) != KERN_SUCCESS)
    {
        SIHD_LOG(error, "OS: current_rss task info");
        return (size_t)0L;
    }
    return (size_t)info.resident_size;

#elif defined(__SIHD_LINUX__)

    long rss = 0L;
    FILE* fp = NULL;
    if ((fp = fopen("/proc/self/statm", "r")) == NULL)
    {
        SIHD_LOG(error, "OS: current_rss fopen: " << strerror(errno));
        return (size_t)0L;
    }
    if (fscanf(fp, "%*s%ld", &rss) != 1)
    {
        fclose(fp);
        SIHD_LOG(error, "OS: current_rss fscanf: " << strerror(errno));
        return (size_t)0L;
    }
    fclose(fp);
    return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);

#else
    return (size_t)0L;
#endif
}

}