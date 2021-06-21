#include <sihd/util/atexit.hpp>
#include <sihd/util/Logger.hpp>
#include <algorithm>

#include <signal.h>
#include <string.h>

namespace sihd::util::atexit
{

NEW_LOGGER("sihd::util::atexit");

bool installed = false;
std::mutex _runnable_mutex;
std::list<IRunnable *> _runnables;

void    add_handler(IRunnable *ptr)
{
    std::lock_guard lock(_runnable_mutex);
    auto it = std::find(_runnables.begin(), _runnables.end(), ptr);
    if (it == _runnables.end())
        _runnables.push_back(ptr);
}

void    remove_handler(IRunnable *ptr)
{
    std::lock_guard lock(_runnable_mutex);
    auto it = std::find(_runnables.begin(), _runnables.end(), ptr);
    if (it != _runnables.end())
        _runnables.erase(it);
}

void    clear_handlers()
{
    std::lock_guard lock(_runnable_mutex);
    for (IRunnable *runnable : _runnables)
    {
        delete runnable;
    }
    _runnables.clear();
}

void    exit_callback()
{
    LOG(debug, "Running exit callbacks");
    std::lock_guard lock(_runnable_mutex);
    for (IRunnable *runnable : _runnables)
    {
        try
        {
            runnable->run();
            delete runnable;
        }
        catch (const std::exception & e)
        {
            LOG(error, "Error while running exit handler: " << e.what());
        }
        catch (...)
        {
            LOG(error, "Error while running exit handler - non standard exception");
        }
    }
    _runnables.clear();
}

bool    install()
{
    if (installed == false)
    {
        int ret = std::atexit(exit_callback);
        if (ret != 0)
        {
            LOG_ERROR("Cannot install atexit handler");
            return false;
        }
        installed = true;
    }
    return installed;
}

std::string get_signal_name(int sig)
{
    char *signame = strsignal(sig);
    if (signame == nullptr)
        return std::to_string(sig);
    return signame;
}

void    signal_callback(int sig)
{
    LOG(debug, "Signal caught: " << get_signal_name(sig));
    exit_callback();
}

bool    handle_signal(int sig)
{
    sighandler_t handler = signal(sig, signal_callback);
    if (handler == SIG_ERR)
    {
        LOG(error, "Error handling signal: " << get_signal_name(sig));
        return false;
    }
    return true;
}

bool    remove_signal(int sig)
{
    sighandler_t handler = signal(sig, SIG_DFL);
    if (handler == SIG_ERR)
    {
        LOG(error, "Error removing signal: " << get_signal_name(sig));
        return false;
    }
    return true;

}

}