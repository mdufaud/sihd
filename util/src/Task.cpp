#include <sihd/util/Task.hpp>
# include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

Task::Task(IRunnable *to_run, std::time_t run_at, std::time_t reschedule_every):
    run_at(run_at), resched_time(reschedule_every), _runnable_ptr(to_run)
{
}

Task::Task(std::function<bool(void)> fun, std::time_t run_at, std::time_t reschedule_every):
    run_at(run_at), resched_time(reschedule_every), _run_method(fun) 
{
    _runnable_ptr = nullptr;
}

Task::~Task()
{
}

bool    Task::run()
{
    if (_runnable_ptr != nullptr)
        return _runnable_ptr->run();
    return _run_method();
}

}