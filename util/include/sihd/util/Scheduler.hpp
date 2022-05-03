#ifndef __SIHD_UTIL_SCHEDULER_HPP__
# define __SIHD_UTIL_SCHEDULER_HPP__

# include <atomic>
# include <list>
# include <map>
# include <thread>
# include <sihd/util/Clocks.hpp>
# include <sihd/util/Named.hpp>
# include <sihd/util/Task.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/time.hpp>
# include <sihd/util/Thread.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/ScopedModifier.hpp>

namespace sihd::util
{

class Scheduler: public Named, public IStoppableRunnable, public Configurable
{
    public:
        Scheduler(const std::string & name, Node *parent = nullptr);
        virtual ~Scheduler();

        virtual bool start();
        virtual bool stop();
        virtual bool is_running() const;

        virtual void pause();
        virtual void resume();

        virtual void add_task(Task *t);
        virtual void remove_task(Task *t);

        virtual void clear_tasks();

        time_t now() const;

        IClock *get_clock() const;
        void set_clock(IClock *clock);

        bool set_as_fast_as_possible(bool active);

        // number of overruns that occured after started
        uint32_t overruns;
        // time after not running a task is considered an overrun
        uint32_t overrun_at;
        // nanoseconds acceptable before a task may be run
        uint32_t acceptable_nano;

    protected:
        bool run();

        void _delete_tasks();
        void _add_to_delete_task(Task *t);
        bool _wait_for_next_task();
        Task *_get_next_task(time_t time);
        virtual void _play_task(Task *task, time_t time);

        std::atomic<bool> _running;
        std::atomic<bool> _paused;
        std::atomic<bool> _waiting;
        std::atomic<bool> _pausing;
        IClock *_clock_ptr;
        std::thread _thread;
        time_t _next_run;
        time_t _begin_run;
        std::mutex _mutex_task;
        std::mutex _mutex_state;
        std::mutex _mutex_play;
        Waitable _waitable_task;
        Waitable _waitable_pause;
        std::list<Task *> _task_rm_list;
        std::multimap<time_t, Task *> _task_map;

        SystemClock _default_clock;
        bool _no_delay;
        time_t _paused_time;
};

}

#endif