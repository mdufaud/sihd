#ifndef __SIHD_UTIL_TIME_HPP__
#define __SIHD_UTIL_TIME_HPP__

#include <chrono>
#include <ctime>
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

time_t get_timezone();
std::string get_timezone_name();

void nsleep(time_t nano);
void usleep(time_t micro);
void msleep(time_t milli);
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
struct timeval milli_to_tv(time_t milliseconds);
static constexpr auto ms_to_tv = milli_to_tv;
static constexpr auto milliseconds_to_tv = milli_to_tv;
// nano -> timeval (micro resolution)
struct timeval to_tv(time_t micro);
// nano -> timeval (nano resolution)
struct timeval to_nano_tv(time_t nano);

/*
 * nano -> statically allocated nano, other call to localtime/gmtime erase previous result
 * may be nullptr if year cannot be contained
 */
struct tm to_tm(time_t nano, bool localtime = false);

// nano -> micro
time_t to_micro(time_t nano);
static constexpr auto to_us = to_micro;
static constexpr auto to_microseconds = to_micro;
// nano -> milli
time_t to_milli(time_t nano);
static constexpr auto to_ms = to_milli;
static constexpr auto to_milliseconds = to_milli;
// nano -> sec
time_t to_sec(time_t nano);
static constexpr auto to_seconds = to_sec;
// nano -> min
time_t to_min(time_t nano);
static constexpr auto to_minutes = to_min;
// nano -> hour
time_t to_hours(time_t nano);
// nano -> day
time_t to_days(time_t nano);
// nano -> seconds,milliseconds
double to_double(time_t nano);
double to_double_milliseconds(time_t nano);
// nano -> hz
double to_freq(time_t nano);
static constexpr auto to_hz = to_freq;
static constexpr auto to_frequency = to_freq;

// chrono -> nano
template <traits::Duration Duration>
constexpr Duration to_duration(time_t nano)
{
    // seconds -> nano / 1E9        = nano / (1E9 / 1) / 1
    // milliseconds -> nano / 1E6   = nano / (1E9 / 1E3) / 1
    // nano -> nano / 1             = nano / (1E9 / 1E9) / 1
    // min -> (nano / 1E9) / 60     = nano / (1E9 / 1) / 60
    return Duration((nano / (std::chrono::duration<int64_t, std::nano>::period::den / Duration::period::den))
                    / Duration::period::num);
}

// micro -> nano
constexpr time_t micro(time_t micro)
{
    return micro * 1E3;
}
static constexpr auto us = micro;
static constexpr auto microseconds = micro;
// milli -> nano
constexpr time_t milli(time_t milli)
{
    return milli * 1E6;
}
static constexpr auto ms = milli;
static constexpr auto milliseconds = milli;
// sec -> nano
constexpr time_t sec(time_t sec)
{
    return sec * 1E9;
}
static constexpr auto seconds = sec;
// min -> nano
constexpr time_t min(time_t min)
{
    return sec(min) * 60;
}
static constexpr auto minutes = min;
// hour -> nano
constexpr time_t hours(time_t hour)
{
    return min(hour) * 60;
}
// day -> nano
constexpr time_t days(time_t day)
{
    return hours(day) * 24;
}
// hz -> nano
constexpr time_t freq(double hz)
{
    if (hz < 0.0)
        return 0;
    return (double)(1. / hz) * 1E9;
}
static constexpr auto frequency = freq;
static constexpr auto hz = freq;
// seconds,milliseconds -> nano
time_t from_double(double sec_milli);
time_t from_double_milliseconds(double milli_micro);

// duration -> nano
template <traits::Duration Duration>
constexpr time_t duration(Duration duration)
{
    // seconds -> count() * 1E9     == count() * (1E9 / 1) * 1
    // nano -> count() * 1          == count() * (1E9 / 1E9) * 1
    // min -> count() * 1E9 * 60    == count() * (1E9 / 1) * 60
    return (duration.count() * (std::chrono::duration<int64_t, std::nano>::period::den / Duration::period::den))
           * Duration::period::num;
}

// timepoint -> nano
template <traits::Duration Duration>
constexpr time_t floor(time_t val)
{
    return val - (val % time::duration<Duration>(Duration {1}));
}

// tm struct -> nano
time_t tm(struct tm & tm);
// tm struct -> nano
time_t local_tm(struct tm & tm);
// timeval with micro resolution -> nano
time_t tv(const struct timeval & tv);
// timeval with nano resolution -> nano
time_t nano_tv(const struct timeval & tv);

bool is_leap_year(int year);

} // namespace time

} // namespace sihd::util

#endif
