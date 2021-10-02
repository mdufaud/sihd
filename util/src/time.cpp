#include <sihd/util/time.hpp>

namespace sihd::util::time
{

double  to_double(time_t sec, time_t usec)
{
    return (double)sec + (usec / 1E6);
}

struct timeval  from_double(double time)
{
    struct timeval ret;

    ret.tv_sec = (time_t)time;
    ret.tv_usec = (double)((time_t)time - time) * 1E6;
    return ret;
}

time_t     to_micro(time_t nano)
{
    return nano / 1E3;
}

time_t     to_milli(time_t nano)
{
    return nano / 1E6;
}

time_t     to_sec(time_t nano)
{
    return nano / 1E9;
}

time_t     to_min(time_t nano)
{
    return to_sec(nano) / 60;
}

time_t     to_hour(time_t nano)
{
    return to_min(nano) / 60;
}

time_t     to_day(time_t nano)
{
    return to_hour(nano) / 24;
}

time_t     micro(time_t t)
{
    return t * 1E3;
}

time_t     milli(time_t t)
{
    return t * 1E6;
}

time_t     sec(time_t t)
{
    return t * 1E9;
}

time_t     min(time_t t)
{
    return sec(t) * 60;
}

time_t     hour(time_t t)
{
    return min(t) * 60;
}

time_t     day(time_t t)
{
    return hour(t) * 24;
}

time_t     freq(double freq)
{
    if (freq == 0.0)
        return 0;
    return (double)(1. / freq) * 1E9;
}

}