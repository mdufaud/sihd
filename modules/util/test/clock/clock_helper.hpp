#ifndef __SIHD_UTIL_CLOCK_HELPER_HPP__
#define __SIHD_UTIL_CLOCK_HELPER_HPP__

#include <atomic>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/time.hpp>

namespace test
{

// clock advancing by a fixed step on every now() call, for deterministic durations
class FakeClock: public sihd::util::IClock
{
    public:
        FakeClock(sihd::util::time::UnixTime step = sihd::util::time::milli(1)): _step(step) {}

        sihd::util::Timestamp now() const override
        {
            _calls += 1;
            return sihd::util::Timestamp(_calls * _step);
        }

        bool is_steady() const override { return true; }
        bool start() override { return true; }
        bool stop() override { return true; }

    private:
        sihd::util::time::UnixTime _step;
        mutable std::atomic<sihd::util::time::UnixTime> _calls = 0;
};

} // namespace test

#endif
