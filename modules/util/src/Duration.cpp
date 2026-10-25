#include <algorithm>
#include <limits>

#include <sihd/util/Duration.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

namespace
{

int saturated_int(time::UnixTime value)
{
    return static_cast<int>(
        std::clamp<time::UnixTime>(value, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
}

} // namespace

std::string Duration::str(bool total_parenthesis, bool nano_resolution) const
{
    return str::timeoffset_str(Timestamp(_nano), total_parenthesis, nano_resolution);
}

int Duration::nanoseconds_int() const
{
    return saturated_int(this->nanoseconds());
}

int Duration::microseconds_int() const
{
    return saturated_int(this->microseconds());
}

int Duration::milliseconds_int() const
{
    return saturated_int(this->milliseconds());
}

int Duration::seconds_int() const
{
    return saturated_int(this->seconds());
}

int Duration::minutes_int() const
{
    return saturated_int(this->minutes());
}

int Duration::hours_int() const
{
    return saturated_int(this->hours());
}

int Duration::days_int() const
{
    return saturated_int(this->days());
}

} // namespace sihd::util
