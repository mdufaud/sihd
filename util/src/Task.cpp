#include <sihd/util/Task.hpp>
# include <sihd/util/Logger.hpp>

namespace sihd::util
{

LOGGER;

Task::Task(): run_at(0), resched_time(0), _runnable_ptr(nullptr)
{
}

Task::Task(IRunnable *to_run, std::time_t run_at, std::time_t reschedule_every):
    run_at(run_at), resched_time(reschedule_every), _runnable_ptr(to_run)
{
}

Task::Task(std::function<bool(void)> fun, std::time_t run_at, std::time_t reschedule_every):
    run_at(run_at), resched_time(reschedule_every), _runnable_ptr(nullptr), _run_method(std::move(fun))
{
    _runnable_ptr = nullptr;
}

Task::~Task()
{
}

void    Task::set_method(std::function<bool(void)> fun)
{
    _run_method = std::move(fun);
}

void    Task::set_runnable(IRunnable *to_run)
{
    _runnable_ptr = to_run;
}

bool    Task::run()
{
    if (_runnable_ptr != nullptr)
        return _runnable_ptr->run();
    else if (_run_method)
        return _run_method();
    return false;
}

}