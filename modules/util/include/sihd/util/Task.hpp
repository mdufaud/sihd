#ifndef __SIHD_UTIL_TASK_HPP__
#define __SIHD_UTIL_TASK_HPP__

#include <ctime>
#include <functional>

#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

// what a periodic task does when it is played late - now is past one or more of its scheduled
// reschedule_time slots:
enum class LatenessPolicy
{
    // replays every missed slot in a burst (default) - one run per missed period, for counting
    // or sampling work
    replay_missed,
    // drops the grid - the next run is scheduled reschedule_time after the current run started,
    // keeping at least reschedule_time between runs
    push_back,
    // drops missed slots - runs once now, then jumps to the next future grid slot, for refresh
    // or heartbeat work where bursting is pointless
    skip_missed,
};

struct TaskOptions
{
        static TaskOptions none() { return TaskOptions {}; }
        // MUTUALLY EXCLUSIVE WITH RUN_IN
        // precise timestamp to run task at
        Timestamp run_at = 0;
        // MUTUALLY EXCLUSIVE WITH RUN_AT
        // task to run in a certain duration
        Duration run_in = 0;
        // reschedule task based on previous time
        Duration reschedule_time = 0;
        LatenessPolicy late_policy = LatenessPolicy::replay_missed;
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

        Timestamp run_at;
        Duration run_in;
        Duration reschedule_time;
        LatenessPolicy late_policy;

    private:
        IRunnable *_runnable_ptr;
        std::function<bool(void)> _run_method;
};

} // namespace sihd::util

#endif