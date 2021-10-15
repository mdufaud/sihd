#ifndef __SIHD_UTIL_WAITABLE_HPP__
# define __SIHD_UTIL_WAITABLE_HPP__

# include <mutex>
# include <condition_variable>
# include <sihd/util/Clocks.hpp>

namespace sihd::util
{

class Waitable
{
    public:
        Waitable();
        virtual ~Waitable();

        void notify(int times);
        void notify_all();

        void infinite_wait();
        bool wait_until(std::time_t nano_timestamp);
        // wait for x nanoseconds -- returns true if timed out
        bool wait_for(std::time_t nano_duration);
        // wait X notifications for a duration
        bool wait_loop(std::time_t nano_duration, uint32_t times);
        // wait for x nanoseconds -- returns time elapsed
        std::time_t wait_elapsed(std::time_t nano_duration);

        void cancel_loop();

    protected:
        std::mutex _mutex;
        std::condition_variable _condition;
        bool _stop_waiting;
};

}

#endif 