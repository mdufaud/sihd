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

std::time_t     SteadyClock::now()
{
    return _clock.now().time_since_epoch().count();
}

bool    SteadyClock::is_steady()
{
    return _clock.is_steady;
}

SystemClock::SystemClock()
{   
}

SystemClock::~SystemClock()
{
}

std::time_t     SystemClock::now()
{
    return _clock.now().time_since_epoch().count();
}

bool    SystemClock::is_steady()
{
    return _clock.is_steady;
}

}