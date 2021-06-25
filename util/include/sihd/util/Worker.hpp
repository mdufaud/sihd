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
# include <sihd/util/Node.hpp>
# include <sihd/util/thread.hpp>

namespace sihd::util
{

/**
 * @brief Kinda meant to be inherited and change step method
 *          though as a backup, can set a method to run
 * 
 */
class Worker:   virtual public IRunnable,
                virtual public ISteppable,
                public Node,
                public Configurable
{
    public:
        Worker(const std::string & name, Node *parent = nullptr);
        virtual ~Worker();

        bool    set_frequency(double freq);
        bool    set_method(std::function<bool(time_t)> method);

        bool    start_thread();
        bool    stop_thread();

        bool    is_running()
        {
            return _running;
        }

        virtual bool    run();
        virtual bool    step(std::time_t delta);

    private:
        bool        _running;
        Waitable    _waitable;
        SteadyClock _clock;
        std::mutex  _worker_mutex;
        std::thread _worker_thread;
        std::time_t _sleep_time;
        std::function<bool(time_t)> _step_method;
};

}

#endif 