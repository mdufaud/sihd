#include <sihd/util/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/atexit.hpp>

#include <vector>
#include <unistd.h>

#include <execinfo.h>
#include <string.h>

#include <map>
#include <list>
#include <signal.h>
#include <algorithm>

#define SIHD_BACKTRACE_SIZE 15

namespace sihd::util::os
{

NEW_LOGGER("sihd::util::os");

void    *load_lib(std::string lib_name)
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

const int backtrace_size = SIHD_BACKTRACE_SIZE;
void *_backtrace_buffer[backtrace_size];

ssize_t  write(int fd, const char *s)
{
    int i = 0;
    while (s[i])
        ++i;
    return ::write(fd, s, i);
}

ssize_t  write_endl(int fd, const char *s)
{
    ssize_t ret = write(fd, s);
    return ret + ::write(fd, "\n", 1);
}

void    write_number(int fd, int number)
{
    char c;
    if (number < 10)
    {
        c = (char)number + '0';
        ::write(fd, &c, 1);
    }
    else
    {
        write_number(fd, number / 10);
        c = (char)(number % 10) + '0';
        ::write(fd, &c, 1);
    }
}

ssize_t    backtrace(int fd)
{
    size_t size = ::backtrace(_backtrace_buffer, backtrace_size);
    char **strings = (char **)backtrace_symbols(_backtrace_buffer, size);
    write(fd, "sihd::util::os::backtrace (");
    write_number(fd, size);
    write_endl(fd, " calls)");
    if (strings == nullptr)
    {
        write_endl(fd, "Error while getting backtrace symbols");
        return -1;
    }
    uint32_t i = 0;
    while (i < size)
    {
        write(fd, "[");
        write_number(fd, i);
        write(fd, "]\t--> ");
        write_endl(fd, strings[i]);
        ++i;
    }
    free(strings);
    return size;
}

bool _signal_used = false;
std::mutex _signal_mutex;
std::map<int, std::list<IRunnable *>>  _map_signals_handlers;

bool    clear_signal_handlers(int sig)
{
    for (IRunnable *runnable : _map_signals_handlers[sig])
    {
        delete runnable;
    }
    _map_signals_handlers[sig].clear();
    return _unhandle_signal(sig);
}

bool    clear_signal_handlers()
{
    std::lock_guard lock(_signal_mutex);
    bool ret = true;
    for (auto & [sig, runnables_lst] : _map_signals_handlers)
    {
        for (IRunnable *runnable : runnables_lst)
        {
            delete runnable;
        }
        runnables_lst.clear();
        if (_unhandle_signal(sig) == false)
            ret = false;
    }
    _map_signals_handlers.clear();
    return ret;
}

bool    clear_signal_handler(int sig, IRunnable *runnable)
{
    std::lock_guard lock(_signal_mutex);
    auto & lst = _map_signals_handlers[sig];
    auto it = std::find(lst.begin(), lst.end(), runnable);
    if (it != lst.end())
    {
        lst.erase(it);
        if (lst.empty())
            return _unhandle_signal(sig);
        return true;
    }
    return false;
}

void    signal_callback(int sig)
{
    LOG(debug, "Signal caught: " << os::get_signal_name(sig));
    std::lock_guard lock(_signal_mutex);
    for (IRunnable *runnable : _map_signals_handlers[sig])
    {
        runnable->run();
    }
}

bool    add_signal_handler(int sig, IRunnable *runnable)
{
    sighandler_t handler = signal(sig, signal_callback);
    if (handler == SIG_ERR)
    {
        LOG(error, "Error handling signal: " << os::get_signal_name(sig));
        return false;
    }
    std::lock_guard lock(_signal_mutex);
    _map_signals_handlers[sig].push_back(runnable);
    if (_signal_used == false)
    {
        atexit::add_handler(new Task([] () -> bool
        {
            clear_signal_handlers();
            return true;
        }));
        atexit::install();
        _signal_used = true;
    }
    return true;
}

bool    _unhandle_signal(int sig)
{
    sighandler_t handler = signal(sig, SIG_DFL);
    if (handler == SIG_ERR)
    {
        LOG(error, "Error removing signal: " << os::get_signal_name(sig));
        return false;
    }
    return true;

}

std::string get_signal_name(int sig)
{
    char *signame = strsignal(sig);
    if (signame == nullptr)
        return std::to_string(sig);
    return signame;
}

}