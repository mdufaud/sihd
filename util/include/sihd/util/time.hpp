#ifndef __SIHD_UTIL_TIME_HPP__
# define __SIHD_UTIL_TIME_HPP__

# include <chrono>

namespace sihd::util::time
{

void nsleep(time_t nano);
void usleep(time_t micro);
void msleep(time_t milli);
void sleep(time_t sec);

double to_double(time_t sec, time_t usec);
struct timeval tv_from_double(double time);
struct timeval tv_from_milli(time_t milliseconds);

time_t to_micro(time_t nano);
time_t to_milli(time_t nano);
time_t to_sec(time_t nano);
time_t to_min(time_t nano);
time_t to_hour(time_t nano);
time_t to_day(time_t nano);

time_t micro(time_t micro);
time_t milli(time_t milli);
time_t sec(time_t sec);
time_t min(time_t min);
time_t hour(time_t hour);
time_t day(time_t day);

time_t freq(double freq);

};

#endif 