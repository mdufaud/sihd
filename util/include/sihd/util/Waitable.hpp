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

        // notify condition x times - when < 0 notifies all
        void    notify(int times = -1);

        // wait until timeout - when < 0 infinite wait -- returns true if timed out
        bool    wait(std::time_t nano_timestamp = -1);
        // wait for x nanoseconds -- returns true if timed out
        bool    wait_for(std::time_t nano_duration);
        // wait X notifications for a duration
        bool    wait_loop(std::time_t nano_duration, uint32_t times);
        // wait for x nanoseconds -- returns time elapsed
        std::time_t wait_elapsed(std::time_t nano_duration);

    protected:
        std::mutex  _mutex;
        std::condition_variable _condition;
        bool _stop_waiting;
};

}

#endif 