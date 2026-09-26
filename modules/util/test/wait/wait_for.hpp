#ifndef __SIHD_UTIL_WAIT_FOR_HPP__
#define __SIHD_UTIL_WAIT_FOR_HPP__

#include <chrono>

#include <sihd/util/time.hpp>

namespace test
{

template <typename Pred>
bool wait_for(Pred && pred,
              std::chrono::milliseconds timeout = std::chrono::milliseconds(10000),
              std::chrono::milliseconds poll_interval = std::chrono::milliseconds(10))
{
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout;
    while (pred() == false)
    {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        sihd::util::time::msleep(poll_interval.count());
    }
    return true;
}

} // namespace test

#endif
