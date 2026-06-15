#ifndef __SIHD_UTIL_DURATION_HPP__
#define __SIHD_UTIL_DURATION_HPP__

#include <string>

#include <sihd/util/TimeBase.hpp>

namespace sihd::util
{

// A relative interval of time (nanosecond resolution). Subset of Timestamp's interface:
// arithmetic, chrono conversions and numeric accessors, without any calendar/wall-clock notion.
class Duration : public TimeBase<Duration>
{
    public:
        using TimeBase<Duration>::TimeBase;

        // human readable interval, eg "1h2m3s"
        std::string str(bool total_parenthesis = false, bool nano_resolution = false) const;
};

} // namespace sihd::util

#endif
