#include <sihd/util/OS.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/AtExit.hpp>

#include <vector>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <algorithm>

// backtrace not available in windows / android
#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__)
# include <execinfo.h>
#endif

// for usage of sighandler_t in windows
#if defined(__SIHD_WINDOWS__)
typedef void (*sighandler_t)(int);
#endif

namespace sihd::util
{

LOGGER;

bool        OS::signal_used = false;
std::mutex  OS::signal_mutex;
std::map<int, std::list<IRunnable *>>  OS::map_signals_handlers;

bool    OS::clear_signal_handlers(int sig)
{
    std::lock_guard lock(OS::signal_mutex);
    for (IRunnable *runnable : OS::map_signals_handlers[sig])
    {
        delete runnable;
    }
    OS::map_signals_handlers[sig].clear();
    return unhandle_signal(sig);
}

bool    OS::clear_signal_handlers()
{
    std::lock_guard lock(OS::signal_mutex);
    bool ret = true;
    for (auto & [sig, runnables_lst] : OS::map_signals_handlers)
    {
        for (IRunnable *runnable : runnables_lst)
        {
            delete runnable;
        }
        runnables_lst.clear();
        if (unhandle_signal(sig) == false)
            ret = false;
    }
    OS::map_signals_handlers.clear();
    return ret;
}

bool    OS::clear_signal_handler(int sig, IRunnable *runnable)
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

void    OS::signal_callback(int sig)
{
    LOG(debug, "Signal caught: " << OS::get_signal_name(sig));
    std::lock_guard lock(OS::signal_mutex);
    for (IRunnable *runnable : OS::map_signals_handlers[sig])
    {
        runnable->run();
    }
}

bool    OS::add_signal_handler(int sig, IRunnable *runnable)
{
    sighandler_t handler = signal(sig, signal_callback);
    if (handler == SIG_ERR)
    {
        LOG(error, "Error handling signal: " << OS::get_signal_name(sig));
        return false;
    }
    std::lock_guard lock(OS::signal_mutex);
    OS::map_signals_handlers[sig].push_back(runnable);
    if (OS::signal_used == false)
    {
        AtExit::add_handler(new Task([] () -> bool
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
        LOG(error, "Error removing signal: " << OS::get_signal_name(sig));
        return false;
    }
    return true;
}

int     OS::ioctl(int fd, unsigned long request, unsigned long *arg_ptr)
{
#if !defined(__SIHD_WINDOWS__)
    return ::ioctl(fd, request, arg_ptr);
#else
    return ::ioctlsocket(fd, request, arg_ptr);
#endif
}

// strsignal does not exists in windows
#if !defined(__SIHD_WINDOWS__)

std::string OS::get_signal_name(int sig)
{
    char *signame = strsignal(sig);
    if (signame == nullptr)
        return std::to_string(sig);
    return signame;
}

#else

std::string OS::get_signal_name(int sig)
{
    return std::to_string(sig);
}

#endif

// backtrace not available in windows / android
#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__)

# ifndef SIHD_BACKTRACE_SIZE
#  define SIHD_BACKTRACE_SIZE 15
# endif

void        *OS::backtrace_buffer[SIHD_BACKTRACE_SIZE];
const int   OS::backtrace_size = SIHD_BACKTRACE_SIZE;

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
    size_t wanted_size = std::min(OS::backtrace_size, SIHD_BACKTRACE_SIZE);
    size_t size = ::backtrace(OS::backtrace_buffer, wanted_size);
    char **strings = (char **)backtrace_symbols(OS::backtrace_buffer, size);
    bool ret = write(fd, "sihd::util::OS::backtrace (") > 0;
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

#endif // end of backtrace


// dlopen not available in windows
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

void *OS::get_symbol_lib(void *handle, const std::string & sym_name)
{
    if (handle == nullptr)
        return nullptr;
    return dlsym(handle, sym_name.c_str());
}

std::string OS::get_error_lib()
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
        LOG(error, "OS: could not load library: " << OS::get_error_lib());
        return nullptr;
    }
    void *sym_ptr = OS::get_symbol_lib(handle, sym_name);
    if (sym_ptr == nullptr)
    {
        LOG(error, "OS: could not load symbol: " << OS::get_error_lib());
        return nullptr;
    }
    if (dlclose(handle) != 0)
        LOG(warning, "OS: could not close lib handle: " << OS::get_error_lib());
    return sym_ptr;
}

#endif // end of shared lib

}