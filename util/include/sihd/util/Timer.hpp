#ifndef __SIHD_UTIL_TIMER_HPP__
#define __SIHD_UTIL_TIMER_HPP__

#include <atomic>
#include <functional>
#include <thread>

#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::util
{

class Timer
{
    public:
        using Callback = std::function<void()>;

        Timer(Callback && callback, Timestamp timeout);
        ~Timer();

        void cancel();

    private:
        std::atomic<bool> _stop;
        Waitable _waitable;
        std::thread _thread;
};

} // namespace sihd::util

#endif
