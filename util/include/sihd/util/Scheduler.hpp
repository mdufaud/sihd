#ifndef __SIHD_UTIL_SCHEDULER_HPP__
# define __SIHD_UTIL_SCHEDULER_HPP__

# include <mutex>
# include <condition_variable>
# include <list>
# include <map>
# include <thread>
# include <sihd/util/Clocks.hpp>
# include <sihd/util/Named.hpp>
# include <sihd/util/Task.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/time.hpp>
# include <sihd/util/thread.hpp>

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

        // Number of overruns
        uint32_t    overruns;
        // Time after not running a task is considered an overrun
        uint32_t    overrun_at;

    protected:
        void    _delete_tasks();
        void    _add_to_delete_task(Task *t);
        bool    _wait_for(std::time_t nano);
        Task    *_get_next_task(std::time_t time);
        void    _play_task(Task *task, std::time_t time);

        bool    _running;
        std::time_t _next_run;
        std::time_t _begin_run;
        std::thread _thread;
        std::mutex  _mutex_task;
        std::mutex  _mutex_run;
        std::mutex  _mutex_wait;
        std::condition_variable _cond;
        std::multimap<std::time_t, Task *>   _task_map;
        std::list<Task *>   _task_rm_list;
        IClock   *_clock_ptr;
        SteadyClock _steady_clock;
};

}

#endif 