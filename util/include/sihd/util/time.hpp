#ifndef __SIHD_UTIL_TIME_HPP__
# define __SIHD_UTIL_TIME_HPP__

# include <chrono>

namespace sihd::util::time
{

std::time_t     to_micro(std::time_t nano);
std::time_t     to_milli(std::time_t nano);
std::time_t     to_sec(std::time_t nano);
std::time_t     to_min(std::time_t nano);
std::time_t     to_hour(std::time_t nano);
std::time_t     to_day(std::time_t nano);

std::time_t     micro(std::time_t micro);
std::time_t     milli(std::time_t milli);
std::time_t     sec(std::time_t sec);
std::time_t     min(std::time_t min);
std::time_t     hour(std::time_t hour);
std::time_t     day(std::time_t day);

std::time_t     freq(double freq);

};

#endif 