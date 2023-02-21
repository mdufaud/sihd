#include <date/date.h>
#include <date/tz.h> // current_zone

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

namespace
{

const date::time_zone *_get_cached_current_zone()
{
    // cache it (expensive call that will not change during the program runtime)
    static const date::time_zone *current_zone = date::current_zone();
    return current_zone;
}

std::chrono::system_clock::duration _get_local_offset(const std::chrono::system_clock::time_point & time_point)
{
    return time_point.time_since_epoch()
           - date::make_zoned(_get_cached_current_zone(), time_point).get_local_time().time_since_epoch();
}

} // namespace

const std::string_view Timestamp::default_format = "%Y/%m/%d %H:%M:%S";

Timestamp::Timestamp(time_t nano): _nano(nano) {}

Timestamp::Timestamp(Clocktime clocktime)
{
    _nano = time::hours(clocktime.hour);
    _nano += time::minutes(clocktime.minute);
    _nano += time::seconds(clocktime.second);
    _nano += time::milliseconds(clocktime.millisecond);
}

Timestamp::Timestamp(Calendar calendar)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = calendar.year - 1900;
    tm.tm_mon = calendar.month - 1;
    tm.tm_mday = calendar.day;
    tm.tm_isdst = -1;

    _nano = time::seconds(mktime(&tm));
}

Timestamp::Timestamp(Calendar calendar, Clocktime clocktime)
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

    _nano = time::seconds(mktime(&tm));
}

Timestamp::Timestamp(std::chrono::nanoseconds duration): _nano(duration.count()) {}

Timestamp::Timestamp(std::chrono::microseconds duration): _nano(time::duration<std::micro>(duration)) {}

Timestamp::Timestamp(std::chrono::milliseconds duration): _nano(time::duration<std::milli>(duration)) {}

Timestamp::Timestamp(std::chrono::seconds duration): _nano(time::duration<std::ratio<1>>(duration)) {}

Timestamp::Timestamp(std::chrono::minutes duration): _nano(time::duration<std::ratio<60>>(duration)) {}

Timestamp::Timestamp(std::chrono::hours duration): _nano(time::duration<std::ratio<3600>>(duration)) {}

Timestamp::~Timestamp() {}

Timestamp Timestamp::now()
{
    return Timestamp(Clock::default_clock.now());
}

bool Timestamp::in_interval(Timestamp from, Timestamp offset) const
{
    return _nano >= from.get() && _nano <= (from.get() + offset.get());
}

std::string Timestamp::timeoffset_str(bool total_parenthesis, bool nano_resolution) const
{
    return str::timeoffset_str(_nano, total_parenthesis, nano_resolution);
}

std::string Timestamp::localtimeoffset_str(bool total_parenthesis, bool nano_resolution) const
{
    return str::localtimeoffset_str(_nano, total_parenthesis, nano_resolution);
}

std::string Timestamp::format(std::string_view fmt) const
{
    return str::format_time(std::abs(_nano), fmt);
}

std::string Timestamp::local_format(std::string_view fmt) const
{
    return str::format_localtime(std::abs(_nano), fmt);
}

std::string Timestamp::str(bool is_local) const
{
    return is_local ? this->format(default_format) : this->local_format(default_format);
}

std::string Timestamp::str_day(bool is_local) const
{
    return is_local ? this->format("%Y/%m/%d") : this->local_format("%Y/%m/%d");
}

std::optional<Timestamp> Timestamp::from_str(std::string_view date_str, std::string_view fmt)
{
    std::istringstream ss {date_str.data()};

    std::chrono::system_clock::time_point time_point;
    ss >> date::parse(fmt.data(), time_point);

    if (ss.fail())
        return std::nullopt;

    return Timestamp {time_point};
}

std::optional<Timestamp> Timestamp::from_local_str(std::string_view date_str, std::string_view fmt)
{
    std::istringstream ss {date_str.data()};

    std::chrono::system_clock::time_point time_point;
    ss >> date::parse(fmt.data(), time_point);

    if (ss.fail())
        return std::nullopt;

    time_point += _get_local_offset(time_point);

    return Timestamp {time_point};
}

Clocktime Timestamp::clocktime() const
{
    struct tm tm = time::to_tm(std::abs(_nano), false);
    return {
        .hour = tm.tm_hour,
        .minute = tm.tm_min,
        .second = tm.tm_sec,
        .millisecond = (int)((_nano - (time::to_seconds(_nano) * 1E9)) / 1E6),
    };
}

Calendar Timestamp::calendar() const
{
    struct tm tm = time::to_tm(std::abs(_nano), false);
    return {.day = tm.tm_mday, .month = tm.tm_mon + 1, .year = tm.tm_year + 1900};
}

Clocktime Timestamp::local_clocktime() const
{
    struct tm tm = time::to_tm(std::abs(_nano), true);
    return {
        .hour = tm.tm_hour,
        .minute = tm.tm_min,
        .second = tm.tm_sec,
        .millisecond = (int)((_nano - (time::to_seconds(_nano) * 1E9)) / 1E6),
    };
}

Calendar Timestamp::local_calendar() const
{
    struct tm tm = time::to_tm(std::abs(_nano), true);
    return {.day = tm.tm_mday, .month = tm.tm_mon + 1, .year = tm.tm_year + 1900};
}

std::string Clocktime::str() const
{
    return fmt::sprintf("%02d:%02d:%02d:%d", hour, minute, second, millisecond);
}

std::string Calendar::str() const
{
    return fmt::sprintf("%02d/%02d/%04d", day, month, year);
}

bool Timestamp::is_leap_year() const
{
    Calendar c = this->local_calendar();
    return time::is_leap_year(c.year);
}

Timestamp::operator time_t() const
{
    return _nano;
}

Timestamp::operator std::chrono::nanoseconds() const
{
    return std::chrono::nanoseconds(_nano);
}

Timestamp::operator std::chrono::microseconds() const
{
    return time::to_duration<std::micro>(_nano);
}

Timestamp::operator std::chrono::milliseconds() const
{
    return time::to_duration<std::milli>(_nano);
}

Timestamp::operator std::chrono::seconds() const
{
    return time::to_duration<std::ratio<1>>(_nano);
}

Timestamp::operator std::chrono::minutes() const
{
    return time::to_duration<std::ratio<60>>(_nano);
}

Timestamp::operator std::chrono::hours() const
{
    return time::to_duration<std::ratio<3600>>(_nano);
}

time_t Timestamp::nanoseconds() const
{
    return _nano;
}

time_t Timestamp::microseconds() const
{
    return time::to_microseconds(_nano);
}

time_t Timestamp::milliseconds() const
{
    return time::to_milliseconds(_nano);
}

time_t Timestamp::seconds() const
{
    return time::to_seconds(_nano);
}

time_t Timestamp::minutes() const
{
    return time::to_minutes(_nano);
}

time_t Timestamp::hours() const
{
    return time::to_hours(_nano);
}

time_t Timestamp::days() const
{
    return time::to_days(_nano);
}

struct timeval Timestamp::tv() const
{
    return time::to_nano_tv(_nano);
}

} // namespace sihd::util
