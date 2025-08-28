#include <algorithm>
#include <cstdlib>
#include <list>
#include <mutex>

#include <sihd/util/AtExit.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::util::atexit
{

namespace
{

bool g_installed = false;
std::mutex g_runnable_mutex;
std::list<IRunnable *> g_runnables;

} // namespace

void add_handler(IRunnable *ptr)
{
    std::lock_guard lock(g_runnable_mutex);
    auto it = std::find(g_runnables.begin(), g_runnables.end(), ptr);
    if (it == g_runnables.end())
        g_runnables.push_back(ptr);
}

void remove_handler(IRunnable *ptr)
{
    std::lock_guard lock(g_runnable_mutex);
    auto it = std::find(g_runnables.begin(), g_runnables.end(), ptr);
    if (it != g_runnables.end())
        g_runnables.erase(it);
}

void clear_handlers()
{
    std::lock_guard lock(g_runnable_mutex);
    for (IRunnable *runnable : g_runnables)
    {
        delete runnable;
    }
    g_runnables.clear();
}

// logger's fprintf not called because stream are flushed clean after exit
void exit_callback()
{
    if (g_installed == false)
        return;
    for (IRunnable *runnable : g_runnables)
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
    g_runnables.clear();
}

bool install()
{
    if (g_installed == false)
    {
        int ret = std::atexit(exit_callback);
        if (ret != 0)
            return false;
        g_installed = true;
    }
    return g_installed;
}

} // namespace sihd::util::atexit
