#include <sihd/util/Clocks.hpp>

namespace sihd::util
{

SystemClock Clock::default_clock;

SteadyClock::SteadyClock()
{
}

SteadyClock::~SteadyClock()
{
}

time_t     SteadyClock::now() const
{
    return _clock.now().time_since_epoch().count();
}

bool    SteadyClock::is_steady() const
{
    return _clock.is_steady;
}

SystemClock::SystemClock()
{
}

SystemClock::~SystemClock()
{
}

time_t     SystemClock::now() const
{
    return _clock.now().time_since_epoch().count();
}

bool    SystemClock::is_steady() const
{
    return _clock.is_steady;
}

}