#include <ratio>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SystemClock Clock::default_clock;

namespace
{

template <typename ClockType>
time::UnixTime clock_now_ns(const ClockType & clk)
{
    if constexpr (std::ratio_equal_v<typename ClockType::period, std::micro>)
        return time::microseconds(clk.now().time_since_epoch().count());
    else
        return std::chrono::duration_cast<std::chrono::nanoseconds>(clk.now().time_since_epoch()).count();
}

} // namespace

time::UnixTime SteadyClock::now() const
{
    return clock_now_ns(_clock);
}

bool SteadyClock::is_steady() const
{
    return _clock.is_steady;
}

time::UnixTime SystemClock::now() const
{
    return clock_now_ns(_clock);
}

bool SystemClock::is_steady() const
{
    return _clock.is_steady;
}

} // namespace sihd::util
