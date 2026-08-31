#ifndef __SIHD_UTIL_SCHEDULER_HPP__
#define __SIHD_UTIL_SCHEDULER_HPP__

#include <atomic>
#include <list>

#include <sihd/util/AWorkerService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

// how the worker thread waits for the next task's deadline:
// - sleep: blocks on a condition variable until the deadline - costs no cpu but wakes tens of
//   microseconds late (kernel wakeup latency)
// - sleep_then_spin: sleeps most of the wait, then busy-polls the clock over the last
//   spin_window so the task starts right on time - costs cpu during that window; a clock that
//   does not advance on its own (virtual clock) would spin forever, so 128 identical reads give
//   up polling and go back to the condition variable
enum class IdlePolicy
{
    sleep,
    sleep_then_spin,
};

/**
 * Plays queued tasks on time from a single worker thread. Tasks run without any lock held and are
 * owned by the scheduler until played - remove_task() hands the ownership back, and returns false
 * for a task currently running: the scheduler keeps ownership of it in that case, do not delete
 * it. A periodic task missing its run time follows TaskOptions::late_policy.
 */
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

        Timestamp now() const;

        IClock *clock() const;
        void set_clock(IClock *clock);

        bool set_no_delay(bool active);

        bool set_idle_policy(IdlePolicy policy);
        IdlePolicy idle_policy() const;

        // time spent polling the clock before the deadline when idle_policy is sleep_then_spin
        bool set_spin_window(Duration window);
        Duration spin_window() const;

        // at start, jump overdue periodic tasks to their next future grid slot
        bool set_skip_missed_on_start(bool active);
        bool skip_missed_on_start() const;

        // number of overruns that occured after started - thread safe
        std::atomic<size_t> overruns;
        // time after not running a task is considered an overrun
        Duration overrun_at;
        // nanoseconds acceptable before a task may be run ahead of time
        Duration acceptable_task_preplay_ns_time;

    protected:
        bool on_work_start() override;
        bool on_work_stop() override;

        Waitable _waitable_task;
        std::vector<Task *> _tasks_to_add;
        std::multimap<Timestamp, Task *> _task_map;

    private:
        void _prepare_tasks();
        void _delete_trashed_tasks();
        void _unprotected_add_task_to_map(Task *task);

        void _wait_for_next_task();
        void _wait_until_deadline(Duration delay, uint64_t seq);
        void _spin_until(Timestamp deadline, uint64_t seq);
        Task *_get_playable_task(Timestamp now);
        void _play_task(Task *task, Timestamp now);

        void _resume_tasks();

        IClock *_clock_ptr;
        Timestamp _paused_time_at;
        std::atomic<bool> _paused;
        Waitable _waitable_pause;

        Timestamp _begin_run;
        bool _tasks_prepared;
        // only pushed by the worker thread
        std::list<Task *> _trash_task_list;
        Timestamp _next_run;
        // bumped on every _task_map mutation
        std::atomic<uint64_t> _task_map_seq;

        IdlePolicy _idle_policy;
        Duration _spin_window;
        bool _skip_missed_on_start;

        SystemClock _default_clock;
        bool _no_delay;
};

} // namespace sihd::util

#endif
