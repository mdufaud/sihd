#include <sihd/util/OS.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/AtExit.hpp>

#include <vector>
#include <unistd.h>

#if !defined(__SIHD_WINDOWS__)
# include <execinfo.h>
#endif

#include <string.h>

#include <signal.h>
#include <algorithm>

#define SIHD_BACKTRACE_SIZE 15

namespace sihd::util
{

LOGGER;

#if !defined(__SIHD_WINDOWS__)

void        *OS::backtrace_buffer[SIHD_BACKTRACE_SIZE];
const int   OS::backtrace_size = SIHD_BACKTRACE_SIZE;

bool        OS::signal_used = false;
std::mutex  OS::signal_mutex;
std::map<int, std::list<IRunnable *>>  OS::map_signals_handlers;

void    *OS::load_lib(std::string lib_name)
{
    std::vector<std::string>    to_try = {
        "lib" + lib_name + ".so",
        lib_name + ".so",
        lib_name,
        lib_name + ".dll"
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
    size_t size = ::backtrace(OS::backtrace_buffer, OS::backtrace_size);
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

std::string OS::get_signal_name(int sig)
{
    char *signame = strsignal(sig);
    if (signame == nullptr)
        return std::to_string(sig);
    return signame;
}

#else

void        *OS::load_lib([[maybe_unused]] std::string lib_name) { return nullptr; }
ssize_t     OS::write([[maybe_unused]] int fd, [[maybe_unused]] const char *s) { return 0; }
ssize_t     OS::write_endl([[maybe_unused]] int fd, [[maybe_unused]] const char *s) { return 0; }
ssize_t     OS::write_number([[maybe_unused]] int fd, [[maybe_unused]] int number) { return 0; }
ssize_t     OS::backtrace([[maybe_unused]] int fd) { return 0; }
bool        OS::clear_signal_handlers([[maybe_unused]] int sig) { return true; }
bool        OS::clear_signal_handlers() { return true; }
bool        OS::clear_signal_handler([[maybe_unused]] int sig, [[maybe_unused]] IRunnable *runnable) { return true; }
void        OS::signal_callback([[maybe_unused]] int sig) {}
bool        OS::add_signal_handler([[maybe_unused]] int sig, [[maybe_unused]] IRunnable *runnable) { return true; }
bool        OS::unhandle_signal([[maybe_unused]] int sig) { return true; }
std::string OS::get_signal_name([[maybe_unused]] int sig) { return ""; }

#endif

}