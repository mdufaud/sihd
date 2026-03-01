#include <sihd/util/Clocks.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

SystemClock Clock::default_clock;

time_t SteadyClock::now() const
{
    // emscripten clock is microseconds
    if constexpr (platform::is_emscripten)
        return time::microseconds(_clock.now().time_since_epoch().count());
    else
        return _clock.now().time_since_epoch().count();
}

bool SteadyClock::is_steady() const
{
    return _clock.is_steady;
}

time_t SystemClock::now() const
{
    // emscripten clock is microseconds
    if constexpr (platform::is_emscripten)
        return time::microseconds(_clock.now().time_since_epoch().count());
    else
        return _clock.now().time_since_epoch().count();
}

bool SystemClock::is_steady() const
{
    return _clock.is_steady;
}

} // namespace sihd::util
