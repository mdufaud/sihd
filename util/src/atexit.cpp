#include <sihd/util/atexit.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/Logger.hpp>
#include <algorithm>

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

// logger's fprintf not called because stream are flushed clean after exit
void    exit_callback()
{
    os::clear_signal_handlers();
    for (IRunnable *runnable : _runnables)
    {
        try
        {
            runnable->run();
        }
        catch (const std::exception & e)
        {
            std::cerr << "Error while running exit handler: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Error while running exit handler - non standard exception" << std::endl;
        }
        delete runnable;
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
            LOG(error, "Cannot install atexit handler");
            return false;
        }
        LOG(info, "Exit handler installed");
        installed = true;
    }
    return installed;
}

}