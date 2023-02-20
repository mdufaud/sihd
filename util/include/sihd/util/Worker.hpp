#ifndef __SIHD_UTIL_WORKER_HPP__
#define __SIHD_UTIL_WORKER_HPP__

#include <atomic>
#include <mutex>
#include <thread>

#include <sihd/util/Configurable.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/forward.hpp>

namespace sihd::util
{

/**
 * @brief Kinda meant to be inherited and change step method
 *          though as a backup, can set a method to run
 *
 */
class Worker: protected IRunnable,
              public Configurable
{
    public:
        Worker(IRunnable *runnable = nullptr);
        virtual ~Worker();

        void set_worker_detach(bool active);
        virtual void set_runnable(IRunnable *runnable);
        virtual bool start_worker(std::string_view name);
        virtual bool start_sync_worker(std::string_view name);
        virtual bool stop_worker();
        virtual bool is_worker_running() const { return _running; }
        virtual bool is_worker_started() const { return _started; }

    protected:
        virtual bool run();
        virtual bool on_worker_start();
        virtual bool on_worker_stop();

        virtual bool _prepare_run();

    private:
        std::string _worker_thread_name;
        IRunnable *_runnable_ptr;

        std::atomic<bool> _started;
        std::atomic<bool> _running;
        bool _detach;
        Synchronizer _synchro;
        std::thread _worker_thread;
        std::mutex _mutex_state;
};

} // namespace sihd::util

#endif