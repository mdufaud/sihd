#include <stdexcept>

#include <sihd/util/Task.hpp>

namespace sihd::util
{

Task::Task(const TaskOptions & options):
    run_at(options.run_at),
    run_in(options.run_in),
    reschedule_time(options.reschedule_time),
    _runnable_ptr(nullptr)
{
    if (run_at > 0 && run_in > 0)
        throw std::logic_error(
            "Cannot have a task running at specific time and running in a specific duration at once");
}

Task::Task(IRunnable *to_run, const TaskOptions & options): Task(options)
{
    _runnable_ptr = to_run;
}

Task::Task(std::function<bool(void)> fun, const TaskOptions & options): Task(options)
{
    _run_method = std::move(fun);
}

Task::~Task() = default;

void Task::set_method(std::function<bool(void)> fun)
{
    _run_method = std::move(fun);
}

void Task::set_runnable(IRunnable *to_run)
{
    _runnable_ptr = to_run;
}

bool Task::run()
{
    if (_runnable_ptr != nullptr)
        return _runnable_ptr->run();
    else if (_run_method)
        return _run_method();
    return false;
}

} // namespace sihd::util