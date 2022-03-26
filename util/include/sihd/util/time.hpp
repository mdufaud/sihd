#ifndef __SIHD_UTIL_TIME_HPP__
# define __SIHD_UTIL_TIME_HPP__

# include <chrono>
# include <time.h>
# include <sys/time.h>

namespace sihd::util::time
{

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
constexpr auto ms_to_tv = time::milli_to_tv;
constexpr auto milliseconds_to_tv = time::milli_to_tv;
// nano -> timeval (micro resolution)
struct timeval to_tv(time_t micro);
// nano -> timeval (nano resolution)
struct timeval to_nano_tv(time_t nano);

/*
* nano -> statically allocated nano, other call to localtime/gmtime erase previous result
* may be nullptr if year cannot be contained
*/
struct tm *to_tm(time_t nano, bool localtime = false);

// nano -> micro
time_t to_micro(time_t nano);
constexpr auto to_us = time::to_micro;
constexpr auto to_microseconds = time::to_micro;
// nano -> milli
time_t to_milli(time_t nano);
constexpr auto to_ms = time::to_milli;
constexpr auto to_milliseconds = time::to_milli;
// nano -> sec
time_t to_sec(time_t nano);
constexpr auto to_seconds = time::to_sec;
// nano -> min
time_t to_min(time_t nano);
constexpr auto to_minutes = time::to_min;
// nano -> hour
time_t to_hours(time_t nano);
// nano -> day
time_t to_days(time_t nano);
// nano -> seconds,milliseconds
double to_double(time_t nano);
// nano -> hz
double to_freq(time_t nano);
constexpr auto to_hz = time::to_freq;
constexpr auto to_frequency = time::to_freq;

// micro -> nano
time_t micro(time_t micro);
constexpr auto us = time::micro;
constexpr auto microseconds = time::micro;
// milli -> nano
time_t milli(time_t milli);
constexpr auto ms = time::milli;
constexpr auto milliseconds = time::milli;
// sec -> nano
time_t sec(time_t sec);
constexpr auto seconds = time::sec;
// min -> nano
time_t min(time_t min);
constexpr auto minutes = time::min;
// hour -> nano
time_t hours(time_t hour);
// day -> nano
time_t days(time_t day);
// hz -> nano
time_t freq(double hz);
constexpr auto frequency = time::freq;
constexpr auto hz = time::freq;
// seconds,milliseconds -> nano
time_t from_double(double sec_milli);

// tm struct -> nano
time_t tm(struct tm *tm);
// timeval with micro resolution -> nano
time_t tv(const struct timeval & tv);
// timeval with nano resolution -> nano
time_t nano_tv(const struct timeval & tv);

};

#endif