#ifndef __SIHD_UTIL_TIMEDHANDLER_HPP__
#define __SIHD_UTIL_TIMEDHANDLER_HPP__

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Stat.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

// forwards every notification to the wrapped handler while timing each call;
// the wrapped handler must outlive it
template <typename T>
class TimedHandler: public IHandler<T *>
{
    public:
        TimedHandler(IHandler<T *> *wrapped, std::string_view label = "", IClock *clock = nullptr):
            _wrapped(wrapped),
            _label(label),
            _clock_ptr(clock)
        {
        }

        void handle(T *sender) override
        {
            if (_active.load() == false)
            {
                _wrapped->handle(sender);
                return;
            }
            const Timestamp begin = this->_now();
            _wrapped->handle(sender);
            const time::UnixTime elapsed = this->_now() - begin;
            {
                std::lock_guard l(_stat_mutex);
                _stat.add_sample(elapsed);
            }
            _last = elapsed;
        }

        IHandler<T *> *wrapped() const { return _wrapped; }

        const std::string & label() const { return _label; }

        // an inactive handler forwards notifications without sampling them
        void set_active(bool active) { _active = active; }

        bool active() const { return _active.load(); }

        Stat<time::UnixTime> stat() const
        {
            std::lock_guard l(_stat_mutex);
            return _stat;
        }

        time::UnixTime last() const { return _last.load(); }

    private:
        Timestamp _now() const { return _clock_ptr != nullptr ? _clock_ptr->now() : _clock.now(); }

        IHandler<T *> *_wrapped;
        std::string _label;
        IClock *_clock_ptr;
        SteadyClock _clock;
        std::atomic<bool> _active {true};
        mutable std::mutex _stat_mutex;
        Stat<time::UnixTime> _stat;
        std::atomic<time::UnixTime> _last {0};
};

} // namespace sihd::util

#endif
