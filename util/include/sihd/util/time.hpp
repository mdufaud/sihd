#ifndef __SIHD_UTIL_TIME_HPP__
# define __SIHD_UTIL_TIME_HPP__

# include <chrono>

namespace sihd::util::time
{

double to_double(time_t sec, time_t usec);
struct timeval from_double(double time);

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