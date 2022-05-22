#include <sihd/util/Time.hpp>

#include <thread>

#include <string.h>

namespace sihd::util
{

std::mutex Time::_unsafe_c_mutex;

void    Time::nsleep(time_t nano)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
}

void    Time::usleep(time_t micro)
{
    std::this_thread::sleep_for(std::chrono::microseconds(micro));
}

void    Time::msleep(time_t milli)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milli));
}

void    Time::sleep(time_t sec)
{
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

double  Time::tv_to_double(const struct timeval & tv)
{
    return (double)tv.tv_sec + (tv.tv_usec / 1E6);
}

double  Time::nano_tv_to_double(const struct timeval & tv)
{
    return (double)tv.tv_sec + (tv.tv_usec / 1E9);
}

double  Time::to_double(time_t nano)
{
    return nano / 1E9;
}

double  Time::to_freq(time_t nano)
{
    return (1 / (double)(nano / 1E9));
}

struct timeval  Time::double_to_tv(double time)
{
    struct timeval ret;

    ret.tv_sec = (time_t)time;
    ret.tv_usec = (time - (time_t)time) * 1E6;
    return ret;
}

struct timeval  Time::double_to_nano_tv(double time)
{
    struct timeval ret;

    ret.tv_sec = (time_t)time;
    ret.tv_usec = (time - (time_t)time) * 1E9;
    return ret;
}

struct timeval  Time::milli_to_tv(time_t milliseconds)
{
    struct timeval ret;

    ret.tv_sec = milliseconds / 1E3;
    ret.tv_usec = (milliseconds % (int)1E3) * 1E3;
    return ret;
}

struct timeval  Time::to_tv(time_t nano)
{
    struct timeval ret;

    ret.tv_sec = nano / 1E6;
    ret.tv_usec = nano % (int)1E6;
    return ret;
}

struct timeval  Time::to_nano_tv(time_t nano)
{
    struct timeval ret;

    ret.tv_sec = nano / 1E9;
    ret.tv_usec = nano % (int)1E9;
    return ret;
}

struct tm Time::to_tm(time_t nano, bool localtime)
{
    std::lock_guard l(_unsafe_c_mutex);
    time_t sec = nano / 1E9;
    // make a copy
    return localtime ? *::localtime(&sec) : *::gmtime(&sec);
}

time_t  Time::to_micro(time_t nano)
{
    return nano / 1E3;
}

time_t  Time::to_milli(time_t nano)
{
    return nano / 1E6;
}

time_t  Time::to_sec(time_t nano)
{
    return nano / 1E9;
}

time_t  Time::to_min(time_t nano)
{
    return to_sec(nano) / 60;
}

time_t  Time::to_hours(time_t nano)
{
    return to_min(nano) / 60;
}

time_t  Time::to_days(time_t nano)
{
    return to_hours(nano) / 24;
}

time_t  Time::micro(time_t t)
{
    return t * 1E3;
}

time_t  Time::milli(time_t t)
{
    return t * 1E6;
}

time_t  Time::sec(time_t t)
{
    return t * 1E9;
}

time_t  Time::min(time_t t)
{
    return sec(t) * 60;
}

time_t  Time::hours(time_t t)
{
    return min(t) * 60;
}

time_t  Time::days(time_t t)
{
    return hours(t) * 24;
}

time_t  Time::from_double(double sec_milli)
{
    time_t sec = (time_t)sec_milli;
    time_t nano = (double)(sec_milli - sec) * 1E9;
    return Time::sec(sec) + nano;
}

time_t  Time::freq(double hz)
{
    if (hz < 0.0)
        return 0;
    return (double)(1. / hz) * 1E9;
}

time_t  Time::tv(const struct timeval & tv)
{
    return (tv.tv_sec * 1E9) + (tv.tv_usec * 1E3);
}

time_t  Time::nano_tv(const struct timeval & tv)
{
    return (tv.tv_sec * 1E9) + tv.tv_usec;
}

time_t  Time::tm(struct tm & tm)
{
    return mktime(&tm) * 1E9;
}

}