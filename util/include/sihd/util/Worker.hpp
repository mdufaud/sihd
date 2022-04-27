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
# include <sihd/util/Waitable.hpp>

namespace sihd::util
{

/**
 * @brief Kinda meant to be inherited and change step method
 *          though as a backup, can set a method to run
 *
 */
class Worker: public IRunnable, public Configurable
{
    public:
        Worker(IRunnable *runnable = nullptr);
        virtual ~Worker();

        void set_worker_detach(bool active);
        virtual void set_runnable(IRunnable *runnable);
        virtual bool start_worker(std::string_view name);
        virtual bool start_sync_worker(std::string_view name);
        virtual bool stop_worker();
        virtual bool is_worker_running() const
        {
            return _running;
        }
        virtual bool is_worker_started() const
        {
            return _started;
        }

    protected:
        virtual bool run();
        virtual bool on_worker_start();
        virtual bool on_worker_stop();

        std::string & _worker_get_name();
        IRunnable *_worker_get_runnable();
        void _worker_set_running(bool active);

    private:
        std::string _worker_thread_name;
        IRunnable *_runnable_ptr;

        bool _started;
        bool _running;
        bool _detach;
        Waitable _running_waitable;
        std::mutex _worker_mutex;
        std::mutex _worker_sync_mutex;
        std::thread _worker_thread;
};

}

#endif