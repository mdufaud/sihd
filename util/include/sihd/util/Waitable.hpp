#ifndef __SIHD_UTIL_WAITABLE_HPP__
#define __SIHD_UTIL_WAITABLE_HPP__

#include <condition_variable>
#include <mutex>

#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/os.hpp>

namespace sihd::util
{

template <typename Mutex, typename ConditionVariable>
class WaitableImpl
{
    public:
        WaitableImpl() = default;
        ~WaitableImpl() = default;

        void notify(int times = 1)
        {
            for (int i = 0; i < times; ++i)
            {
                _condition.notify_one();
            }
        }
        void notify_all() { _condition.notify_all(); }

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

        // predicate must return false to keep waiting - return the last value of the predicate
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

        // predicate must return false to keep waiting - return the last value of the predicate
        template <class Predicate>
        bool wait_until(Timestamp timestamp, Predicate pred_stop_waiting)
        {
            std::unique_lock lock(_mutex);
#if defined(__SIHD_EMSCRIPTEN__)
            // emscripten clock is microseconds
            return _condition.wait_until(lock,
                                         std::chrono::system_clock::time_point(std::chrono::microseconds(timestamp)),
                                         pred_stop_waiting);
#else
            return _condition.wait_until(lock,
                                         std::chrono::system_clock::time_point(std::chrono::nanoseconds(timestamp)),
                                         pred_stop_waiting);
#endif
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        Timestamp wait_until_elapsed(Timestamp timestamp, Predicate pred_stop_waiting)
        {
            Stopwatch watch;
            this->wait_until(timestamp, pred_stop_waiting);
            return watch.time();
        }

        void wait()
        {
            std::unique_lock lock(_mutex);
            _condition.wait(lock);
        }

        /**
         * @brief wait a notification until a timestamp is reached
         *
         * @param timestamp timestamp to reach
         * @return true if timedout
         * @return false if notification came
         */
        bool wait_until(Timestamp timestamp)
        {
            std::unique_lock lock(_mutex);
            // emscripten clock is microseconds
#if defined(__SIHD_EMSCRIPTEN__)
            return _condition.wait_until(lock,
                                         std::chrono::system_clock::time_point(std::chrono::microseconds(timestamp)))
                   == std::cv_status::timeout;
#else
            return _condition.wait_until(lock,
                                         std::chrono::system_clock::time_point(std::chrono::nanoseconds(timestamp)))
                   == std::cv_status::timeout;
#endif
        }

        /**
         * @brief wait a notification for a duration
         *
         * @param duration duration to wait
         * @return true if timedout
         * @return false if notification came
         */
        bool wait_for(Timestamp duration)
        {
            std::unique_lock lock(_mutex);
            return _condition.wait_for(lock, std::chrono::nanoseconds(duration)) == std::cv_status::timeout;
        }

        // waits - returns time elapsed
        Timestamp wait_elapsed()
        {
            Stopwatch hg;
            this->wait();
            return hg.time();
        }
        // waits until timestamp - returns time elapsed
        Timestamp wait_until_elapsed(Timestamp timestamp)
        {
            Stopwatch hg;
            this->wait_until(timestamp);
            return hg.time();
        }
        // wait for duration -- returns time elapsed
        Timestamp wait_for_elapsed(Timestamp duration)
        {
            Stopwatch hg;
            this->wait_for(duration);
            return hg.time();
        }

        Mutex & mutex() { return _mutex; }
        std::lock_guard<Mutex> guard() { return std::lock_guard(_mutex); }

    protected:
        Mutex _mutex;
        ConditionVariable _condition;
};

using Waitable = WaitableImpl<std::mutex, std::condition_variable>;
using WaitableRecursive = WaitableImpl<std::recursive_mutex, std::condition_variable_any>;

} // namespace sihd::util

#endif
