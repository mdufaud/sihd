#ifndef __SIHD_UTIL_SAFEQUEUE_HPP__
#define __SIHD_UTIL_SAFEQUEUE_HPP__

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

namespace sihd::util
{

template <typename T>
class SafeQueue
{
    public:
        SafeQueue(): _terminated(false) {}

        ~SafeQueue() { this->terminate(); }

        bool push(const T & value, size_t max_size = 0)
        {
            std::unique_lock lock(_mutex);

            const bool can_push = max_size == 0 || max_size < _queue.size();
            if (!can_push)
                return false;
            _queue.push(value);

            lock.unlock();

            _cv_push.notify_one();
            return true;
        }

        bool push(T && value, size_t max_size = 0)
        {
            std::unique_lock lock(_mutex);

            const bool can_push = max_size == 0 || _queue.size() < max_size;
            if (!can_push)
                return false;
            _queue.emplace(std::forward<T>(value));

            lock.unlock();

            _cv_push.notify_one();
            return true;
        }

        std::optional<T> try_pop()
        {
            std::unique_lock lock(_mutex);

            if (_queue.empty() || _terminated)
                return std::nullopt;

            T ret = std::move(_queue.front());
            _queue.pop();

            lock.unlock();

            _cv_pop.notify_one();
            return ret;
        }

        T pop()
        {
            std::unique_lock lock(_mutex);

            _cv_push.wait(lock, [this] { return _terminated || _queue.empty() == false; });
            if (_terminated)
            {
                throw std::invalid_argument("Queue is terminated");
            }

            T ret = std::move(_queue.front());
            _queue.pop();

            lock.unlock();

            _cv_pop.notify_one();
            return ret;
        }

        bool wait_for_space(size_t queue_size) const
        {
            std::unique_lock lock(_mutex);
            _cv_pop.wait(lock, [this, queue_size] { return _terminated || _queue.size() < queue_size; });
            return _queue.size() < queue_size;
        }

        void terminate()
        {
            _terminated = true;

            this->clear();

            _cv_push.notify_all();
            _cv_pop.notify_all();
        }

        const T & front() const
        {
            std::lock_guard l(_mutex);
            return _queue.front();
        }

        const T & back() const
        {
            std::lock_guard l(_mutex);
            return _queue.back();
        }

        size_t size() const
        {
            std::lock_guard l(_mutex);
            return _queue.size();
        }

        bool empty() const { return this->size() == 0; }

        void clear()
        {
            std::lock_guard l(_mutex);
            _queue = {};
        }

    protected:

    private:
        std::atomic<bool> _terminated;
        std::queue<T> _queue;
        mutable std::mutex _mutex;
        mutable std::condition_variable _cv_push;
        mutable std::condition_variable _cv_pop;
};

} // namespace sihd::util

#endif
