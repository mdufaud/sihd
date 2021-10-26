#ifndef __SIHD_UTIL_TASK_HPP__
# define __SIHD_UTIL_TASK_HPP__

# include <sihd/util/IRunnable.hpp>
# include <functional>
# include <ctime>

namespace sihd::util
{

class Task: public IRunnable
{
    public:
        Task(IRunnable *to_run,
                std::time_t timestamp_to_run = 0,
                std::time_t reschedule_every = 0);
        Task(std::function<bool(void)> fun,
                std::time_t timestamp_to_run = 0,
                std::time_t reschedule_every = 0);
        virtual ~Task();

        std::time_t run_at;
        std::time_t resched_time;

        virtual bool    run();

    private:
        IRunnable   *_runnable_ptr;
        std::function<bool(void)>   _run_method;
};

}

#endif 