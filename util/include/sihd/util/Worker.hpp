#ifndef __SIHD_UTIL_WORKER_HPP__
# define __SIHD_UTIL_WORKER_HPP__

# include <thread>
# include <functional>
# include <sihd/util/IRunnable.hpp>
# include <sihd/util/ISteppable.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Clocks.hpp>
# include <sihd/util/time.hpp>
# include <sihd/util/Thread.hpp>

namespace sihd::util
{

/**
 * @brief Kinda meant to be inherited and change step method
 *          though as a backup, can set a method to run
 * 
 */
class Worker: virtual public IRunnable, public Configurable
{
    public:
        Worker(IRunnable *runnable = nullptr);
        virtual ~Worker();

        void set_runnable(IRunnable *runnable);
        bool start_worker(const std::string & name);
        bool stop_worker();

        bool is_worker_running()
        {
            return _running;
        }

        virtual bool run();

    protected:
        virtual bool on_worker_start();
        virtual bool on_worker_stop();

        std::string _worker_thread_name;
        IRunnable *_runnable_ptr;

    private:
        bool        _running;
        std::mutex  _worker_mutex;
        std::thread _worker_thread;
};

}

#endif 