#ifndef __SIHD_UTIL_SAFEQUEUE_HPP__
#define __SIHD_UTIL_SAFEQUEUE_HPP__

#include <atomic>
#include <optional>
#include <queue>
#include <stdexcept>

#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

template <typename T>
class SafeQueue
{
    public:
        SafeQueue(): _terminated(false) {}

        ~SafeQueue() { this->terminate(); }

        bool push(const T & value, size_t max_size = 0) { return this->_push(T(value), max_size); }

        bool push(T && value, size_t max_size = 0) { return this->_push(std::move(value), max_size); }

        std::optional<T> try_pop()
        {
            std::optional<T> ret;
            {
                auto l = _waitable.guard();
                if (_queue.empty() || _terminated)
                    return std::nullopt;
                ret.emplace(std::move(_queue.front()));
                _queue.pop();
            }
            _waitable.notify_all();
            return ret;
        }

        T pop()
        {
            auto l = _waitable.wait_guard([this] { return _terminated || _queue.empty() == false; });
            if (_terminated)
                throw std::invalid_argument("Queue is terminated");
            T ret = std::move(_queue.front());
            _queue.pop();
            l.unlock();
            _waitable.notify_all();
            return ret;
        }

        bool wait_for_space(size_t queue_size) const
        {
            auto l = _waitable.wait_guard([this, queue_size] { return _terminated || _queue.size() < queue_size; });
            return _queue.size() < queue_size;
        }

        void terminate()
        {
            {
                auto l = _waitable.guard();
                _terminated = true;
                _queue = {};
            }
            _waitable.notify_all();
        }

        const T & front() const
        {
            auto l = _waitable.guard();
            return _queue.front();
        }

        const T & back() const
        {
            auto l = _waitable.guard();
            return _queue.back();
        }

        size_t size() const
        {
            auto l = _waitable.guard();
            return _queue.size();
        }

        bool empty() const { return this->size() == 0; }

        void clear()
        {
            auto l = _waitable.guard();
            _queue = {};
        }

    private:
        bool _push(T && value, size_t max_size)
        {
            {
                auto l = _waitable.guard();
                if (max_size != 0 && _queue.size() >= max_size)
                    return false;
                _queue.push(std::move(value));
            }
            _waitable.notify_all();
            return true;
        }

        std::atomic<bool> _terminated;
        std::queue<T> _queue;
        mutable Waitable _waitable;
};

} // namespace sihd::util

#endif
