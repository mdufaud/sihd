#include <sihd/util/time.hpp>
#include <thread>

namespace sihd::util::time
{

void nsleep(time_t nano)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
}

void usleep(time_t micro)
{
    std::this_thread::sleep_for(std::chrono::microseconds(micro));
}

void msleep(time_t milli)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milli));
}

void sleep(time_t sec)
{
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

double  to_double(time_t sec, time_t usec)
{
    return (double)sec + (usec / 1E6);
}

struct timeval  tv_from_double(double time)
{
    struct timeval ret;

    ret.tv_sec = (time_t)time;
    ret.tv_usec = (double)(time - (time_t)time) * 1E6;
    return ret;
}

struct timeval  tv_from_milli(time_t milliseconds)
{
    struct timeval ret;

    ret.tv_sec = milliseconds / 1E3;
    ret.tv_usec = (milliseconds % 1000) * 1E3;
    return ret;
}

struct timeval  to_tv(time_t nano)
{
    struct timeval ret;

    ret.tv_sec = nano / 1E9;
    ret.tv_usec = nano % 1000000;
    return ret;
}

time_t tv_to_nano(const struct timeval & tv)
{
    return (tv.tv_sec * 1E9) + (tv.tv_usec * 1E3);
}

time_t nano_tv_to_nano(const struct timeval & tv)
{
    return (tv.tv_sec * 1E9) + tv.tv_usec;
}

time_t to_micro(time_t nano)
{
    return nano / 1E3;
}

time_t to_milli(time_t nano)
{
    return nano / 1E6;
}

time_t to_sec(time_t nano)
{
    return nano / 1E9;
}

time_t to_min(time_t nano)
{
    return to_sec(nano) / 60;
}

time_t to_hour(time_t nano)
{
    return to_min(nano) / 60;
}

time_t to_day(time_t nano)
{
    return to_hour(nano) / 24;
}

time_t micro(time_t t)
{
    return t * 1E3;
}

time_t milli(time_t t)
{
    return t * 1E6;
}

time_t sec(time_t t)
{
    return t * 1E9;
}

time_t min(time_t t)
{
    return sec(t) * 60;
}

time_t hour(time_t t)
{
    return min(t) * 60;
}

time_t day(time_t t)
{
    return hour(t) * 24;
}

time_t freq(double freq)
{
    if (freq < 0.0)
        return 0;
    return (double)(1. / freq) * 1E9;
}

}