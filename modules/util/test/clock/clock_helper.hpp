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

// clock returning a settable instant, for exact time arithmetic
class ManualClock: public sihd::util::IClock
{
    public:
        ManualClock(sihd::util::Timestamp now = sihd::util::Timestamp(0)): _now(now.get()) {}

        sihd::util::Timestamp now() const override
        {
            calls.fetch_add(1, std::memory_order_relaxed);
            return sihd::util::Timestamp(_now.load());
        }

        void set(sihd::util::Timestamp now) { _now.store(now.get()); }
        void advance(sihd::util::time::UnixTime nano) { _now.fetch_add(nano); }

        bool is_steady() const override { return true; }
        bool start() override { return true; }
        bool stop() override { return true; }

        mutable std::atomic<int> calls = 0;

    private:
        std::atomic<sihd::util::time::UnixTime> _now;
};

} // namespace test

#endif
