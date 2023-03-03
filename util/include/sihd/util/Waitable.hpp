#ifndef __SIHD_UTIL_WAITABLE_HPP__
#define __SIHD_UTIL_WAITABLE_HPP__

#include <condition_variable>
#include <mutex>

#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class Waitable
{
    public:
        Waitable();
        virtual ~Waitable();

        void notify(int times = 1);
        void notify_all();

        // predicate must return false to keep notifying
        template <class Predicate>
        void notify_until_predicate(Predicate p, Timestamp wait_time, uint32_t max_notifications = 0)
        {
            uint32_t notifications = 0;

            while (p() == false && (max_notifications == 0 || notifications < max_notifications))
            {
                this->notify(1);
                ++notifications;
                time::nsleep(wait_time);
            }
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        void wait_predicate(Predicate p)
        {
            std::unique_lock lock(_mutex);
            _condition.wait(lock, p);
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        Timestamp wait_until_predicate(Timestamp timeout,
                                       Predicate p,
                                       Timestamp poll_frequency = std::chrono::milliseconds(1))
        {
            time_t total_waited = 0;

            while (_stop_waiting == false && p() == false && (timeout < 0 || total_waited < timeout))
            {
                total_waited += this->wait_for_elapsed(poll_frequency);
            }

            return {total_waited};
        }

        template <class Predicate>
        Timestamp infinite_wait_until_predicate(Predicate p, Timestamp poll_frequency = std::chrono::milliseconds(1))
        {
            return this->wait_until_predicate(-1, std::move(p), poll_frequency);
        }

        void infinite_wait();
        Timestamp infinite_wait_elapsed();

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
        Timestamp wait_for_elapsed(Timestamp t);

    protected:
        std::mutex _mutex;
        std::condition_variable _condition;
        bool _stop_waiting;
};

} // namespace sihd::util

#endif
