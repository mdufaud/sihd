#ifndef __SIHD_UTIL_TASK_HPP__
# define __SIHD_UTIL_TASK_HPP__

# include <functional>
# include <ctime>

# include <sihd/util/IRunnable.hpp>
# include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class Task: public IRunnable
{
    public:
        Task();
        Task(IRunnable *to_run,
                Timestamp timestamp_to_run = 0,
                Timestamp reschedule_every = 0);
        Task(std::function<bool(void)> fun,
                Timestamp timestamp_to_run = 0,
                Timestamp reschedule_every = 0);
        virtual ~Task();

        bool run();
        void set_method(std::function<bool(void)> fun);
        void set_runnable(IRunnable *to_run);

        time_t run_at;
        time_t resched_time;

    private:
        IRunnable *_runnable_ptr;
        std::function<bool(void)> _run_method;
};

}

#endif