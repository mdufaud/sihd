#ifndef __SIHD_UTIL_WAITABLE_HPP__
# define __SIHD_UTIL_WAITABLE_HPP__

# include <mutex>
# include <condition_variable>

#include <sihd/util/Timestamp.hpp>

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

        /**
         * @brief wait a notification for a duration
         *
         * @param t duration to wait
         * @return true if timedout
         * @return false if notification came
         */
        bool wait_for(Timestamp t);

        /**
         * @brief wait X notifications for a duration
         *
         * @param t total duration to wait
         * @param times needed number of notify
         * @return true if timedout
         * @return false if all notifications came in time
         */
        bool wait_for_loop(Timestamp t, uint32_t times);

        // cancel wait_for_loop
        void cancel_loop();

        // wait for duration -- returns time elapsed
        time_t wait_for_elapsed(Timestamp t);

    protected:
        std::mutex _mutex;
        std::condition_variable _condition;
        bool _stop_waiting;
};

}

#endif