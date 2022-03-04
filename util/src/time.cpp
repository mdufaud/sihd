#include <sihd/util/time.hpp>
#include <thread>

#include <iostream>
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

double tv_to_double(const struct timeval & tv)
{
    return (double)tv.tv_sec + (tv.tv_usec / 1E6);
}

double nano_tv_to_double(const struct timeval & tv)
{
    return (double)tv.tv_sec + (tv.tv_usec / 1E9);
}

double to_double(time_t nano)
{
    return nano / 1E9;
}

double to_freq(time_t nano)
{
    return (1 / (double)(nano / 1E9));
}

struct timeval double_to_tv(double time)
{
    struct timeval ret;

    ret.tv_sec = (time_t)time;
    ret.tv_usec = (time - (time_t)time) * 1E6;
    return ret;
}

struct timeval double_to_nano_tv(double time)
{
    struct timeval ret;

    ret.tv_sec = (time_t)time;
    ret.tv_usec = (time - (time_t)time) * 1E9;
    return ret;
}

struct timeval milli_to_tv(time_t milliseconds)
{
    struct timeval ret;

    ret.tv_sec = milliseconds / 1E3;
    ret.tv_usec = (milliseconds % (int)1E3) * 1E3;
    return ret;
}

struct timeval to_tv(time_t nano)
{
    struct timeval ret;

    ret.tv_sec = nano / 1E6;
    ret.tv_usec = nano % (int)1E6;
    return ret;
}

struct timeval to_nano_tv(time_t nano)
{
    struct timeval ret;

    ret.tv_sec = nano / 1E9;
    ret.tv_usec = nano % (int)1E9;
    return ret;
}

struct tm *to_tm(time_t nano, bool localtime)
{
    time_t sec = nano / 1E9;
    return localtime ? ::localtime(&sec) : ::gmtime(&sec);
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

time_t hours(time_t t)
{
    return min(t) * 60;
}

time_t days(time_t t)
{
    return hours(t) * 24;
}

time_t freq(double hz)
{
    if (hz < 0.0)
        return 0;
    return (double)(1. / hz) * 1E9;
}

time_t tv(const struct timeval & tv)
{
    return (tv.tv_sec * 1E9) + (tv.tv_usec * 1E3);
}

time_t nano_tv(const struct timeval & tv)
{
    return (tv.tv_sec * 1E9) + tv.tv_usec;
}

time_t tm(struct tm *tm)
{
    time_t ret = 0;
    if (tm != nullptr)
        ret = mktime(tm) * 1E9;
    return ret;
}

}