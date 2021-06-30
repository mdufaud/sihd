#ifndef __SIHD_UTIL_SCHEDULER_HPP__
# define __SIHD_UTIL_SCHEDULER_HPP__

# include <list>
# include <map>
# include <thread>
# include <sihd/util/Clocks.hpp>
# include <sihd/util/Named.hpp>
# include <sihd/util/Task.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/time.hpp>
# include <sihd/util/Thread.hpp>
# include <sihd/util/Waitable.hpp>

namespace sihd::util
{

class Scheduler:    public Named,
                    virtual public IRunnable
{
    public:
        Scheduler(const std::string & name, Node *parent = nullptr);
        virtual ~Scheduler();

        bool    start();
        bool    stop();

        void    add_task(Task *t);
        void    remove_task(Task *t);

        IClock  *get_clock();
        void    set_clock(IClock *clock);
        bool    is_running();

        virtual bool    run();

        // number of overruns that occured after started
        uint32_t    overruns;
        // time after not running a task is considered an overrun
        uint32_t    overrun_at;
        // nanoseconds acceptable before a task may be run
        uint32_t    acceptable_nano;

    protected:
        void    _delete_tasks();
        void    _add_to_delete_task(Task *t);
        bool    _wait_for(std::time_t nano);
        Task    *_get_next_task(std::time_t time);
        void    _play_task(Task *task, std::time_t time);

        bool    _running;
        IClock  *_clock_ptr;
        std::thread _thread;
        std::time_t _next_run;
        std::time_t _begin_run;
        std::mutex  _mutex_task;
        std::mutex  _mutex_run;
        Waitable    _waitable;
        SteadyClock _steady_clock;
        std::list<Task *>   _task_rm_list;
        std::multimap<std::time_t, Task *>  _task_map;
};

}

#endif 