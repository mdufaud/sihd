#ifndef __SIHD_UTIL_SAFEQUEUE_HPP__
#define __SIHD_UTIL_SAFEQUEUE_HPP__

#include <expected>
#include <optional>
#include <queue>

#include <sihd/util/Error.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

template <typename T>
class SafeQueue
{
    public:
        SafeQueue(): _terminated(false) {}

        SafeQueue(const SafeQueue &) = delete;
        SafeQueue & operator=(const SafeQueue &) = delete;

        ~SafeQueue() { this->terminate(); }

        bool push(const T & value, size_t max_size = 0)
        {
            return this->push_lazy([&] { return value; }, max_size);
        }

        bool push(T && value, size_t max_size = 0)
        {
            return this->push_lazy([&] { return std::move(value); }, max_size);
        }

        // make is not called on refusal, max_size = 0 is unbounded
        template <typename Make>
        bool push_lazy(Make make, size_t max_size = 0)
        {
            bool wake_consumers;
            {
                auto l = _waitable.guard();
                if (_terminated)
                    return false;
                if (max_size != 0 && _queue.size() >= max_size)
                    return false;
                wake_consumers = _queue.empty();
                _queue.push(make());
            }
            // consumers recheck the predicate under the lock before sleeping: only the empty
            // to non empty transition can leave a sleeping one behind
            if (wake_consumers)
                _waitable.notify_all();
            return true;
        }

        // drains what a termination left behind
        std::optional<T> try_pop()
        {
            std::optional<T> ret;
            {
                auto l = _waitable.guard();
                if (_queue.empty())
                    return std::nullopt;
                ret.emplace(std::move(_queue.front()));
                _queue.pop();
            }
            _waitable.notify_all();
            return ret;
        }

        // a closed and drained queue reports ErrorCode::closed
        std::expected<T, Error> pop()
        {
            auto l = _waitable.wait_guard([this] { return _terminated || _queue.empty() == false; });
            if (_queue.empty())
                return std::unexpected(Error(ErrorCode::closed, "queue is closed and drained"));
            T ret = std::move(_queue.front());
            _queue.pop();
            l.unlock();
            _waitable.notify_all();
            return ret;
        }

        // nullopt once terminated and drained, the drain loop pop
        std::optional<T> pop_wait()
        {
            auto l = _waitable.wait_guard([this] { return _terminated || _queue.empty() == false; });
            if (_queue.empty())
                return std::nullopt;
            std::optional<T> ret = std::move(_queue.front());
            _queue.pop();
            l.unlock();
            _waitable.notify_all();
            return ret;
        }

        bool wait_for_space(size_t max_size) const
        {
            auto l = _waitable.wait_guard([this, max_size] { return _terminated || _queue.size() < max_size; });
            return _queue.size() < max_size;
        }

        // pops drain what is left, pop reports closed once drained
        void terminate()
        {
            {
                auto l = _waitable.guard();
                _terminated = true;
            }
            _waitable.notify_all();
        }

        T front() const
        {
            auto l = _waitable.guard();
            return _queue.front();
        }

        T back() const
        {
            auto l = _waitable.guard();
            return _queue.back();
        }

        // visit runs under the lock, reentrancy deadlocks
        template <typename Visit>
        bool peek_front(Visit visit) const
        {
            auto l = _waitable.guard();
            if (_queue.empty())
                return false;
            visit(_queue.front());
            return true;
        }

        template <typename Visit>
        bool peek_back(Visit visit) const
        {
            auto l = _waitable.guard();
            if (_queue.empty())
                return false;
            visit(_queue.back());
            return true;
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
        bool _terminated;
        std::queue<T> _queue;
        mutable Waitable _waitable;
};

} // namespace sihd::util

#endif
