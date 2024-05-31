#ifndef __SIHD_UTIL_WAITABLE_HPP__
#define __SIHD_UTIL_WAITABLE_HPP__

#include <condition_variable>
#include <mutex>

#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

class Waitable
{
    public:
        Waitable();
        ~Waitable();

        void notify(int times = 1);
        void notify_all();

        // predicate must return false to keep waiting
        template <class Predicate>
        void wait(Predicate pred_stop_waiting)
        {
            std::unique_lock lock(_mutex);
            _condition.wait(lock, pred_stop_waiting);
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        Timestamp wait_elapsed(Predicate pred_stop_waiting)
        {
            Stopwatch watch;
            this->wait(pred_stop_waiting);
            return watch.time();
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        bool wait_for(Timestamp duration, Predicate pred_stop_waiting)
        {
            std::unique_lock lock(_mutex);
            return _condition.wait_for(lock, std::chrono::nanoseconds(duration), pred_stop_waiting);
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        Timestamp wait_for_elapsed(Timestamp duration, Predicate pred_stop_waiting)
        {
            Stopwatch watch;
            this->wait_for(duration, pred_stop_waiting);
            return watch.time();
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        bool wait_until(Timestamp timestamp, Predicate pred_stop_waiting)
        {
            std::unique_lock lock(_mutex);
            return _condition.wait_until(lock,
                                         std::chrono::system_clock::time_point(std::chrono::nanoseconds(timestamp)),
                                         pred_stop_waiting);
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        Timestamp wait_until_elapsed(Timestamp timestamp, Predicate pred_stop_waiting)
        {
            Stopwatch watch;
            this->wait_until(timestamp, pred_stop_waiting);
            return watch.time();
        }

        void wait();

        /**
         * @brief wait a notification until a timestamp is reached
         *
         * @param timestamp timestamp to reach
         * @return true if timedout
         * @return false if notification came
         */
        bool wait_until(Timestamp timestamp);

        /**
         * @brief wait a notification for a duration
         *
         * @param duration duration to wait
         * @return true if timedout
         * @return false if notification came
         */
        bool wait_for(Timestamp duration);

        // cancel wait_for_loop
        void cancel_loop();

        // waits - returns time elapsed
        Timestamp wait_elapsed();
        // waits until timestamp - returns time elapsed
        Timestamp wait_until_elapsed(Timestamp timestamp);
        // wait for duration -- returns time elapsed
        Timestamp wait_for_elapsed(Timestamp duration);

        std::mutex & mutex();
        std::lock_guard<std::mutex> guard();

    protected:
        std::mutex _mutex;
        std::condition_variable _condition;
};

} // namespace sihd::util

#endif
