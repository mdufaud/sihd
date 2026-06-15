#ifndef __SIHD_UTIL_TIME_HPP__
#define __SIHD_UTIL_TIME_HPP__

#include <chrono>
#include <ctime>
#include <string>
#include <sys/time.h> // struct timeval (POSIX, not guaranteed by <ctime>)

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

// tv (micro resolution) -> seconds,milliseconds
double tv_to_double(const struct timeval & tv);
// tv (nano resolution) -> seconds,milliseconds
double nano_tv_to_double(const struct timeval & tv);

// seconds,milliseconds -> timeval
struct timeval double_to_tv(double time);
// seconds,milliseconds -> timeval
struct timeval double_to_nano_tv(double time);
// milliseconds -> timeval (micro resolution)
struct timeval milli_to_tv(UnixTime milliseconds);
static constexpr auto ms_to_tv = milli_to_tv;
static constexpr auto milliseconds_to_tv = milli_to_tv;
// nano -> timeval (micro resolution)
struct timeval to_tv(UnixTime micro);
// nano -> timeval (nano resolution)
struct timeval to_nano_tv(UnixTime nano);

/*
 * nano -> statically allocated nano, other call to localtime/gmtime erase previous result
 * may be nullptr if year cannot be contained
 */
struct tm to_tm(UnixTime nano, bool localtime = false);

// nano -> micro
UnixTime to_micro(UnixTime nano);
static constexpr auto to_us = to_micro;
static constexpr auto to_microseconds = to_micro;
// nano -> milli
UnixTime to_milli(UnixTime nano);
static constexpr auto to_ms = to_milli;
static constexpr auto to_milliseconds = to_milli;
// nano -> sec
UnixTime to_sec(UnixTime nano);
static constexpr auto to_seconds = to_sec;
// nano -> min
UnixTime to_min(UnixTime nano);
static constexpr auto to_minutes = to_min;
// nano -> hour
UnixTime to_hours(UnixTime nano);
// nano -> day
UnixTime to_days(UnixTime nano);
// nano -> seconds,milliseconds
double to_double(UnixTime nano);
double to_double_milliseconds(UnixTime nano);
// nano -> hz
double to_freq(UnixTime nano);
static constexpr auto to_hz = to_freq;
static constexpr auto to_frequency = to_freq;

// chrono -> nano
template <traits::Duration Duration>
constexpr Duration to_duration(UnixTime nano)
{
    // seconds -> nano / 1E9        = nano / (1E9 / 1) / 1
    // milliseconds -> nano / 1E6   = nano / (1E9 / 1E3) / 1
    // nano -> nano / 1             = nano / (1E9 / 1E9) / 1
    // min -> (nano / 1E9) / 60     = nano / (1E9 / 1) / 60
    return Duration((nano / (std::chrono::duration<int64_t, std::nano>::period::den / Duration::period::den))
                    / Duration::period::num);
}

// micro -> nano
constexpr UnixTime micro(UnixTime micro)
{
    return micro * 1E3;
}
static constexpr auto us = micro;
static constexpr auto microseconds = micro;
// milli -> nano
constexpr UnixTime milli(UnixTime milli)
{
    return milli * 1E6;
}
static constexpr auto ms = milli;
static constexpr auto milliseconds = milli;
// sec -> nano
constexpr UnixTime sec(UnixTime sec)
{
    return sec * 1E9;
}
static constexpr auto seconds = sec;
// min -> nano
constexpr UnixTime min(UnixTime min)
{
    return sec(min) * 60;
}
static constexpr auto minutes = min;
// hour -> nano
constexpr UnixTime hours(UnixTime hour)
{
    return min(hour) * 60;
}
// day -> nano
constexpr UnixTime days(UnixTime day)
{
    return hours(day) * 24;
}
// hz -> nano
constexpr UnixTime freq(double hz)
{
    if (hz < 0.0)
        return 0;
    return (double)(1. / hz) * 1E9;
}
static constexpr auto frequency = freq;
static constexpr auto hz = freq;
// seconds,milliseconds -> nano
UnixTime from_double(double sec_milli);
UnixTime from_double_milliseconds(double milli_micro);

// duration -> nano
template <traits::Duration Duration>
constexpr UnixTime duration(Duration duration)
{
    // seconds -> count() * 1E9     == count() * (1E9 / 1) * 1
    // nano -> count() * 1          == count() * (1E9 / 1E9) * 1
    // min -> count() * 1E9 * 60    == count() * (1E9 / 1) * 60
    return (duration.count()
            * (std::chrono::duration<int64_t, std::nano>::period::den / Duration::period::den))
           * Duration::period::num;
}

// timepoint -> nano
template <traits::Duration Duration>
constexpr UnixTime floor(UnixTime val)
{
    return val - (val % time::duration<Duration>(Duration {1}));
}

// tm struct -> nano
UnixTime tm(struct tm & tm);
// tm struct -> nano
UnixTime local_tm(struct tm & tm);
// timeval with micro resolution -> nano
UnixTime tv(const struct timeval & tv);
// timeval with nano resolution -> nano
UnixTime nano_tv(const struct timeval & tv);

UnixTime ts(const struct timespec & ts);
struct timespec to_ts(UnixTime nano);

bool is_leap_year(int year);

} // namespace time

} // namespace sihd::util

#endif
