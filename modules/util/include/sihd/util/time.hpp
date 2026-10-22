#ifndef __SIHD_UTIL_TIME_HPP__
#define __SIHD_UTIL_TIME_HPP__

#include <sys/time.h> // struct timeval (POSIX, not guaranteed by <ctime>)

#include <chrono>
#include <ctime>
#include <limits>
#include <string>

#include <sihd/util/traits.hpp>

namespace sihd::util
{

namespace date
{

using days = std::chrono::duration<int64_t, std::ratio<86400>>;
using weeks = std::chrono::duration<int64_t, std::ratio<604800>>;
using months = std::chrono::duration<int64_t, std::ratio<2629746>>;
using years = std::chrono::duration<int64_t, std::ratio<31556952>>;

} // namespace date

namespace time
{

// nanosecond count (absolute epoch time or relative duration)
using UnixTime = int64_t;

time_t get_timezone();
std::string get_timezone_name();

void nsleep(UnixTime nano);
void usleep(UnixTime micro);
void msleep(UnixTime milli);
void sleep(time_t sec);

double tv_to_double(const struct timeval & tv);
double nano_tv_to_double(const struct timeval & tv);

struct timeval double_to_tv(double time);
struct timeval double_to_nano_tv(double time);
struct timeval milli_to_tv(UnixTime milliseconds);
static constexpr auto ms_to_tv = milli_to_tv;
static constexpr auto milliseconds_to_tv = milli_to_tv;
struct timeval to_tv(UnixTime micro);
// nano -> timeval holding nanoseconds in tv_usec [0, 1E9): only round-trips through nano_tv
struct timeval to_nano_tv(UnixTime nano);

/*
 * nano -> statically allocated nano, other call to localtime/gmtime erase previous result
 * may be nullptr if year cannot be contained
 */
struct tm to_tm(UnixTime nano, bool localtime = false);

UnixTime to_micro(UnixTime nano);
static constexpr auto to_us = to_micro;
static constexpr auto to_microseconds = to_micro;
UnixTime to_milli(UnixTime nano);
static constexpr auto to_ms = to_milli;
static constexpr auto to_milliseconds = to_milli;
UnixTime to_sec(UnixTime nano);
static constexpr auto to_seconds = to_sec;
UnixTime to_min(UnixTime nano);
static constexpr auto to_minutes = to_min;
UnixTime to_hours(UnixTime nano);
UnixTime to_days(UnixTime nano);
// the running-unit clock components (the Clocktime fields), positive on negative timestamps
int clock_hour(UnixTime nano);
int clock_minute(UnixTime nano);
int clock_second(UnixTime nano);
int clock_millisecond(UnixTime nano);
int clock_microsecond(UnixTime nano);
double to_double(UnixTime nano);
double to_double_milliseconds(UnixTime nano);
double to_freq(UnixTime nano);
static constexpr auto to_hz = to_freq;
static constexpr auto to_frequency = to_freq;

template <traits::Duration Duration>
constexpr Duration to_duration(UnixTime nano)
{
    // unit divisor: (1E9 / unit); minutes additionally divide by 60
    return Duration((nano / (std::chrono::duration<int64_t, std::nano>::period::den / Duration::period::den))
                    / Duration::period::num);
}

constexpr UnixTime micro(UnixTime micro)
{
    return micro * 1000;
}
static constexpr auto us = micro;
static constexpr auto microseconds = micro;
constexpr UnixTime milli(UnixTime milli)
{
    return milli * 1000000;
}
static constexpr auto ms = milli;
static constexpr auto milliseconds = milli;
constexpr UnixTime sec(UnixTime sec)
{
    return sec * 1000000000;
}
static constexpr auto seconds = sec;
constexpr UnixTime min(UnixTime min)
{
    return sec(min) * 60;
}
static constexpr auto minutes = min;
constexpr UnixTime hours(UnixTime hour)
{
    return min(hour) * 60;
}
constexpr UnixTime days(UnixTime day)
{
    return hours(day) * 24;
}
constexpr UnixTime freq(double hz)
{
    if (hz <= 0.0)
        return 0;
    const double period_nano = (1. / hz) * 1E9;
    if (period_nano >= (double)std::numeric_limits<UnixTime>::max())
        return std::numeric_limits<UnixTime>::max();
    return (UnixTime)period_nano;
}
static constexpr auto frequency = freq;
static constexpr auto hz = freq;
UnixTime from_double(double sec_milli);
UnixTime from_double_milliseconds(double milli_micro);

template <traits::Duration Duration>
constexpr UnixTime duration(Duration duration)
{
    // unit multiplier: (1E9 / unit); minutes additionally multiply by 60
    return (duration.count() * (std::chrono::duration<int64_t, std::nano>::period::den / Duration::period::den))
           * Duration::period::num;
}

template <traits::Duration Duration>
constexpr UnixTime floor(UnixTime val)
{
    return val - (val % time::duration<Duration>(Duration {1}));
}

UnixTime tm(struct tm & tm);
UnixTime local_tm(struct tm & tm);
UnixTime tv(const struct timeval & tv);
UnixTime nano_tv(const struct timeval & tv);

UnixTime ts(const struct timespec & ts);
struct timespec to_ts(UnixTime nano);

bool is_leap_year(int year);

} // namespace time

} // namespace sihd::util

#endif
