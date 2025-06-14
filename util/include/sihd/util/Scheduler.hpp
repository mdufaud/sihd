#ifndef __SIHD_UTIL_SCHEDULER_HPP__
#define __SIHD_UTIL_SCHEDULER_HPP__

#include <atomic>
#include <list>
// #include <map>
#include <queue>
#include <thread>

#include <sihd/util/AWorkerService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

class Scheduler: public Named,
                 public AWorkerService,
                 public Configurable
{
    public:
        Scheduler(const std::string & name, Node *parent = nullptr);
        ~Scheduler();

        void pause();
        void resume();

        /**
         * If a task has a run_at set, it will be played at this exact time, if the scheduler is paused and
         * resumed past that timestamp, it will be played as soon as possible. If a task has a run_in set, it
         * will be played relative to the scheduler's unpaused run time
         */
        void add_task(Task *t);
        bool remove_task(Task *t);
        void clear_tasks();

        time_t now() const;

        IClock *clock() const;
        void set_clock(IClock *clock);

        bool set_no_delay(bool active);

        // number of overruns that occured after started
        size_t overruns;
        // time after not running a task is considered an overrun
        time_t overrun_at;
        // nanoseconds acceptable before a task may be run ahead of time
        time_t acceptable_task_preplay_ns_time;

    protected:
        bool on_work_start() override;
        bool on_work_stop() override;

        Waitable _waitable_task;
        std::vector<Task *> _tasks_to_add;
        std::multimap<time_t, Task *> _task_map;

    private:
        void _prepare_tasks();
        void _add_task_to_trash(Task *t);
        void _delete_trashed_tasks();
        void _unprotected_add_task_to_map(Task *task);

        void _wait_for_next_task();
        Task *_get_playable_task(time_t now);
        void _play_task(Task *task, time_t now);

        void _resume_tasks();

        IClock *_clock_ptr;
        time_t _paused_time_at;
        std::atomic<bool> _paused;
        Waitable _waitable_pause;

        time_t _begin_run;
        bool _tasks_prepared;
        std::list<Task *> _trash_task_list;
        time_t _next_run;

        SystemClock _default_clock;
        bool _no_delay;
};

} // namespace sihd::util

#endif
