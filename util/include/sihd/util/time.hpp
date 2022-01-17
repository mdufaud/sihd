#ifndef __SIHD_UTIL_TIME_HPP__
# define __SIHD_UTIL_TIME_HPP__

# include <chrono>

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
// nano -> milli
time_t to_milli(time_t nano);
// nano -> sec
time_t to_sec(time_t nano);
// nano -> min
time_t to_min(time_t nano);
// nano -> hour
time_t to_hour(time_t nano);
// nano -> day
time_t to_day(time_t nano);
// nano -> seconds,milliseconds
double to_double(time_t nano);

// micro -> nano
time_t micro(time_t micro);
// milli -> nano
time_t milli(time_t milli);
// sec -> nano
time_t sec(time_t sec);
// min -> nano
time_t min(time_t min);
// hour -> nano
time_t hour(time_t hour);
// day -> nano
time_t day(time_t day);
// hz -> nano
time_t freq(double hz);

// tm struct -> nano
time_t tm(struct tm *tm);
// timeval with micro resolution -> nano
time_t tv(const struct timeval & tv);
// timeval with nano resolution -> nano
time_t nano_tv(const struct timeval & tv);

};

#endif