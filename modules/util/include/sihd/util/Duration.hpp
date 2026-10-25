#ifndef __SIHD_UTIL_DURATION_HPP__
#define __SIHD_UTIL_DURATION_HPP__

#include <string>

#include <sihd/util/TimeBase.hpp>

namespace sihd::util
{

// A relative interval of time (nanosecond resolution). Subset of Timestamp's interface:
// arithmetic, chrono conversions and numeric accessors, without any calendar/wall-clock notion.
class Duration: public TimeBase<Duration>
{
    public:
        using TimeBase<Duration>::TimeBase;

        // human readable interval, eg "1h2m3s"
        std::string str(bool total_parenthesis = false, bool nano_resolution = false) const;

        // saturated to int, the system call timeout type: a negative duration stays negative
        int nanoseconds_int() const;
        int microseconds_int() const;
        int milliseconds_int() const;
        int seconds_int() const;
        int minutes_int() const;
        int hours_int() const;
        int days_int() const;
};

} // namespace sihd::util

#endif
