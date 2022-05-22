#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Str.hpp>

namespace sihd::util
{

Timestamp::Timestamp(time_t nano): _nano(nano)
{
}

Timestamp::Timestamp(std::chrono::nanoseconds duration): _nano(duration.count())
{
}

Timestamp::Timestamp(std::chrono::microseconds duration): _nano(time::duration<std::micro>(duration))
{
}

Timestamp::Timestamp(std::chrono::milliseconds duration): _nano(time::duration<std::milli>(duration))
{
}

Timestamp::Timestamp(std::chrono::seconds duration): _nano(time::duration<std::ratio<1>>(duration))
{
}

Timestamp::Timestamp(std::chrono::minutes duration): _nano(time::duration<std::ratio<60>>(duration))
{
}

Timestamp::Timestamp(std::chrono::hours duration): _nano(time::duration<std::ratio<3600>>(duration))
{
}

Timestamp::~Timestamp()
{
}

std::string     Timestamp::gmtime_str(bool total_parenthesis, bool nano_resolution)
{
    return Str::gmtime_str(_nano, total_parenthesis, nano_resolution);
}

std::string     Timestamp::localtime_str(bool total_parenthesis, bool nano_resolution)
{
    return Str::localtime_str(_nano, total_parenthesis, nano_resolution);
}

Timestamp::ClockTime Timestamp::clocktime() const
{
    struct tm *tm = time::to_tm(std::abs(_nano), true);
    return {
        .second = tm->tm_sec,
        .minute = tm->tm_min,
        .hour = tm->tm_hour
    };
}

Timestamp::Calendar Timestamp::calendar() const
{
    struct tm *tm = time::to_tm(std::abs(_nano), true);
    return {
        .day = tm->tm_mday,
        .month = tm->tm_mon + 1,
        .year = tm->tm_year + 1900
    };
}

std::string Timestamp::ClockTime::str() const
{
    return Str::format("%02d:%02d:%02d", hour, minute, second);
}

std::string Timestamp::Calendar::str() const
{
    return Str::format("%02d/%02d/%04d", day, month, year);
}

}