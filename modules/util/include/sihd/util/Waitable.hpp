#ifndef __SIHD_UTIL_WAITABLE_HPP__
#define __SIHD_UTIL_WAITABLE_HPP__

#include <condition_variable>
#include <mutex>
#include <utility>

#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/Timestamp.hpp>

namespace sihd::util
{

template <typename Mutex, typename ConditionVariable>
class WaitableImpl
{
    public:
        // the lock is held whether the predicate was satisfied or timed out
        struct TimedGuard
        {
                std::unique_lock<Mutex> lock;
                bool timed_out;
        };

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

        // waits for the predicate then returns holding the mutex: mutations that must be
        // atomic with the wakeup happen under it
        template <class Predicate>
        std::unique_lock<Mutex> wait_guard(Predicate pred_stop_waiting)
        {
            std::unique_lock lock(_mutex);
            _condition.wait(lock, pred_stop_waiting);
            return lock;
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        Duration wait_elapsed(Predicate pred_stop_waiting)
        {
            Stopwatch watch;
            this->wait(pred_stop_waiting);
            return watch.time();
        }

        // predicate must return false to keep waiting - return the last value of the predicate
        template <class Predicate>
        bool wait_for(Duration duration, Predicate pred_stop_waiting)
        {
            std::unique_lock lock(_mutex);
            return _condition.wait_for(lock, std::chrono::nanoseconds(duration), pred_stop_waiting);
        }

        template <class Predicate>
        TimedGuard wait_for_guard(Duration duration, Predicate pred_stop_waiting)
        {
            std::unique_lock lock(_mutex);
            const bool satisfied = _condition.wait_for(lock, std::chrono::nanoseconds(duration), pred_stop_waiting);
            return TimedGuard {std::move(lock), satisfied == false};
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        Duration wait_for_elapsed(Duration duration, Predicate pred_stop_waiting)
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
            return _condition.wait_until(lock, this->_time_point(timestamp), pred_stop_waiting);
        }

        template <class Predicate>
        TimedGuard wait_until_guard(Timestamp timestamp, Predicate pred_stop_waiting)
        {
            std::unique_lock lock(_mutex);
            const bool satisfied = _condition.wait_until(lock, this->_time_point(timestamp), pred_stop_waiting);
            return TimedGuard {std::move(lock), satisfied == false};
        }

        // predicate must return false to keep waiting
        template <class Predicate>
        Duration wait_until_elapsed(Timestamp timestamp, Predicate pred_stop_waiting)
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
            return _condition.wait_until(lock, this->_time_point(timestamp)) == std::cv_status::timeout;
        }

        /**
         * @brief wait a notification for a duration
         *
         * @param duration duration to wait
         * @return true if timedout
         * @return false if notification came
         */
        bool wait_for(Duration duration)
        {
            std::unique_lock lock(_mutex);
            return _condition.wait_for(lock, std::chrono::nanoseconds(duration)) == std::cv_status::timeout;
        }

        // waits - returns time elapsed
        Duration wait_elapsed()
        {
            Stopwatch hg;
            this->wait();
            return hg.time();
        }
        // waits until timestamp - returns time elapsed
        Duration wait_until_elapsed(Timestamp timestamp)
        {
            Stopwatch hg;
            this->wait_until(timestamp);
            return hg.time();
        }
        // wait for duration -- returns time elapsed
        Duration wait_for_elapsed(Duration duration)
        {
            Stopwatch hg;
            this->wait_for(duration);
            return hg.time();
        }

        Mutex & mutex() { return _mutex; }
        std::lock_guard<Mutex> guard() { return std::lock_guard(_mutex); }

    private:
        static std::chrono::system_clock::time_point _time_point(Timestamp timestamp)
        {
            return std::chrono::system_clock::time_point(
                std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds(timestamp)));
        }

    protected:
        Mutex _mutex;
        ConditionVariable _condition;
};

using Waitable = WaitableImpl<std::mutex, std::condition_variable>;
using WaitableRecursive = WaitableImpl<std::recursive_mutex, std::condition_variable_any>;

} // namespace sihd::util

#endif
