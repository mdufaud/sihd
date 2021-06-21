#include <sihd/util/time.hpp>

namespace sihd::util::time
{

std::time_t     to_micro(std::time_t nano)
{
    return nano / 1E3;
}

std::time_t     to_milli(std::time_t nano)
{
    return nano / 1E6;
}

std::time_t     to_sec(std::time_t nano)
{
    return nano / 1E9;
}

std::time_t     to_min(std::time_t nano)
{
    return to_sec(nano) / 60;
}

std::time_t     to_hour(std::time_t nano)
{
    return to_min(nano) / 60;
}

std::time_t     to_day(std::time_t nano)
{
    return to_hour(nano) / 24;
}

std::time_t     micro(std::time_t t)
{
    return t * 1E3;
}

std::time_t     milli(std::time_t t)
{
    return t * 1E6;
}

std::time_t     sec(std::time_t t)
{
    return t * 1E9;
}

std::time_t     min(std::time_t t)
{
    return sec(t) * 60;
}

std::time_t     hour(std::time_t t)
{
    return min(t) * 60;
}

std::time_t     day(std::time_t t)
{
    return hour(t) * 24;
}

std::time_t     freq(double freq)
{
    if (freq == 0.0)
        return 0;
    return (double)(1. / freq) * 1E9;
}

}