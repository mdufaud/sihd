#ifndef __SIHD_UTIL_TASK_HPP__
#define __SIHD_UTIL_TASK_HPP__

#include <ctime>
#include <functional>

#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

struct TaskOptions
{
        static TaskOptions none() { return TaskOptions {}; }
        // MUTUALLY EXCLUSIVE WITH RUN_IN
        // precise timestamp to run task at
        Timestamp run_at = 0;
        // MUTUALLY EXCLUSIVE WITH RUN_AT
        // task to run in a certain duration
        Timestamp run_in = 0;
        // reschedule task based on previous time
        Timestamp reschedule_time = 0;
};

class Task: public IRunnable
{
    public:
        Task(const TaskOptions & options = TaskOptions::none());
        Task(IRunnable *to_run, const TaskOptions & options = TaskOptions::none());
        Task(std::function<bool(void)> fun, const TaskOptions & options = TaskOptions::none());
        virtual ~Task();

        virtual bool run();
        void set_method(std::function<bool(void)> fun);
        void set_runnable(IRunnable *to_run);

        time_t run_at;
        time_t run_in;
        time_t reschedule_time;

    private:
        IRunnable *_runnable_ptr;
        std::function<bool(void)> _run_method;
};

} // namespace sihd::util

#endif