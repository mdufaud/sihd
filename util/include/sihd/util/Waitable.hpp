#ifndef __SIHD_UTIL_WAITABLE_HPP__
# define __SIHD_UTIL_WAITABLE_HPP__

# include <mutex>
# include <condition_variable>
# include <sihd/util/Clocks.hpp>
# include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class Waitable
{
    public:
        Waitable();
        virtual ~Waitable();

        void notify(int times);
        void notify_all();

        template <class Predicate>
        void wait_predicate(Predicate p)
        {
            std::unique_lock lock(_mutex);
            _condition.wait(lock, p);
        }

        void infinite_wait();
        time_t infinite_wait_elapsed();
        bool wait_until(Timestamp t);
        // wait for x nanoseconds -- returns true if timed out
        bool wait_for(Timestamp t);
        // wait X notifications for a duration
        bool wait_loop(Timestamp t, uint32_t times);
        // cancel wait loop
        void cancel_loop();
        // wait for x nanoseconds -- returns time elapsed
        time_t wait_for_elapsed(Timestamp t);

    protected:
        std::mutex _mutex;
        std::condition_variable _condition;
        bool _stop_waiting;
};

}

#endif