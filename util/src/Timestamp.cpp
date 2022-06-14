#include <sihd/util/Timestamp.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Str.hpp>

namespace sihd::util
{

Timestamp::Timestamp(time_t nano): _nano(nano)
{
}

Timestamp::Timestamp(ClockTime clocktime)
{
    _nano = Time::hours(clocktime.hour);
    _nano += Time::minutes(clocktime.minute);
    _nano += Time::seconds(clocktime.second);
    _nano += Time::milliseconds(clocktime.millisecond);
}

Timestamp::Timestamp(Calendar calendar)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = calendar.year - 1900;
    tm.tm_mon = calendar.month - 1;
    tm.tm_mday = calendar.day;
    tm.tm_isdst = -1;

    _nano = Time::seconds(mktime(&tm));
}

Timestamp::Timestamp(Calendar calendar, ClockTime clocktime)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = calendar.year - 1900;
    tm.tm_mon = calendar.month - 1;
    tm.tm_mday = calendar.day;
    tm.tm_hour = clocktime.hour;
    tm.tm_min = clocktime.minute;
    tm.tm_sec = clocktime.second;
    tm.tm_isdst = -1;

    _nano = Time::seconds(mktime(&tm));
}

Timestamp::Timestamp(std::chrono::nanoseconds duration): _nano(duration.count())
{
}

Timestamp::Timestamp(std::chrono::microseconds duration): _nano(Time::duration<std::micro>(duration))
{
}

Timestamp::Timestamp(std::chrono::milliseconds duration): _nano(Time::duration<std::milli>(duration))
{
}

Timestamp::Timestamp(std::chrono::seconds duration): _nano(Time::duration<std::ratio<1>>(duration))
{
}

Timestamp::Timestamp(std::chrono::minutes duration): _nano(Time::duration<std::ratio<60>>(duration))
{
}

Timestamp::Timestamp(std::chrono::hours duration): _nano(Time::duration<std::ratio<3600>>(duration))
{
}

Timestamp::~Timestamp()
{
}

bool    Timestamp::in_interval(Timestamp from, Timestamp offset) const
{
    return _nano >= from.get() && _nano <= (from.get() + offset.get());
}

std::string     Timestamp::timeoffset_str(bool total_parenthesis, bool nano_resolution) const
{
    return Str::timeoffset_str(_nano, total_parenthesis, nano_resolution);
}

std::string     Timestamp::localtimeoffset_str(bool total_parenthesis, bool nano_resolution) const
{
    return Str::localtimeoffset_str(_nano, total_parenthesis, nano_resolution);
}

std::string     Timestamp::format(std::string_view fmt) const
{
    return Str::format_time(std::abs(_nano), fmt);
}

std::string     Timestamp::local_format(std::string_view fmt) const
{
    return Str::format_localtime(std::abs(_nano), fmt);
}

ClockTime   Timestamp::clocktime() const
{
    struct tm tm = Time::to_tm(std::abs(_nano), false);
    return {
        .hour = tm.tm_hour,
        .minute = tm.tm_min,
        .second = tm.tm_sec,
        .millisecond = (int)((_nano - (Time::to_seconds(_nano) * 1E9)) / 1E6),
    };
}

Calendar    Timestamp::calendar() const
{
    struct tm tm = Time::to_tm(std::abs(_nano), false);
    return {
        .day = tm.tm_mday,
        .month = tm.tm_mon + 1,
        .year = tm.tm_year + 1900
    };
}

ClockTime   Timestamp::local_clocktime() const
{
    struct tm tm = Time::to_tm(std::abs(_nano), true);
    return {
        .hour = tm.tm_hour,
        .minute = tm.tm_min,
        .second = tm.tm_sec,
        .millisecond = (int)((_nano - (Time::to_seconds(_nano) * 1E9)) / 1E6),
    };
}

Calendar    Timestamp::local_calendar() const
{
    struct tm tm = Time::to_tm(std::abs(_nano), true);
    return {
        .day = tm.tm_mday,
        .month = tm.tm_mon + 1,
        .year = tm.tm_year + 1900
    };
}

std::string     ClockTime::str() const
{
    return Str::format("%02d:%02d:%02d:%d", hour, minute, second, millisecond);
}

std::string     Calendar::str() const
{
    return Str::format("%02d/%02d/%04d", day, month, year);
}

bool    Timestamp::is_leap_year() const
{
    Calendar c = this->local_calendar();
    return Time::is_leap_year(c.year);
}

}