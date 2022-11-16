#include <algorithm>

#include <sihd/util/AtExit.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util
{

SIHD_LOGGER;

bool AtExit::installed = false;
std::mutex AtExit::runnable_mutex;
std::list<IRunnable *> AtExit::runnables;

void    AtExit::add_handler(IRunnable *ptr)
{
    std::lock_guard lock(runnable_mutex);
    auto it = std::find(runnables.begin(), runnables.end(), ptr);
    if (it == runnables.end())
        runnables.push_back(ptr);
}

void    AtExit::remove_handler(IRunnable *ptr)
{
    std::lock_guard lock(runnable_mutex);
    auto it = std::find(runnables.begin(), runnables.end(), ptr);
    if (it != runnables.end())
        runnables.erase(it);
}

void    AtExit::clear_handlers()
{
    std::lock_guard lock(runnable_mutex);
    for (IRunnable *runnable: runnables)
    {
        delete runnable;
    }
    runnables.clear();
}

// logger's fprintf not called because stream are flushed clean after exit
void    AtExit::exit_callback()
{
    if (installed == false)
        return ;
    for (IRunnable *runnable: runnables)
    {
        try
        {
            runnable->run();
        }
        catch (const std::exception & e)
        {
            SIHD_CERR("AtExit: error while running exit handler: {}\n", e.what());
        }
        catch (...)
        {
           SIHD_CERR("AtExit: error while running exit handler - non standard exception\n");
        }
        delete runnable;
    }
    runnables.clear();
}

bool    AtExit::install()
{
    if (installed == false)
    {
        int ret = std::atexit(exit_callback);
        if (ret != 0)
        {
            SIHD_LOG(error, "AtExit: cannot install atexit handler");
            return false;
        }
        installed = true;
    }
    return installed;
}

}
