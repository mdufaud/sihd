#include <sys/time.h>

#include <iomanip>
#include <locale>
#include <sstream>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/str.hpp>

namespace sihd::util
{

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

std::string Timestamp::format(std::string_view format) const
{
    return str::format_time(std::abs(_nano), format);
}

std::string Timestamp::local_format(std::string_view format) const
{
    return str::format_localtime(std::abs(_nano), format);
}

std::string Timestamp::format(std::string_view format, const std::locale & loc) const
{
    return str::format_time(std::abs(_nano), format, loc);
}

std::string Timestamp::local_format(std::string_view format, const std::locale & loc) const
{
    return str::format_localtime(std::abs(_nano), format, loc);
}

std::string Timestamp::sec_str(std::string_view format) const
{
    return str::format_time(this->floor<std::chrono::seconds>(), format);
}

std::string Timestamp::local_sec_str(std::string_view format) const
{
    return str::format_localtime(this->floor<std::chrono::seconds>(), format);
}

std::string Timestamp::day_str(std::string_view format) const
{
    return str::format_time(this->floor<date::days>(), format);
}

std::string Timestamp::local_day_str(std::string_view format) const
{
    return str::format_localtime(this->floor<date::days>(), format);
}

std::string Timestamp::str() const
{
    return this->format(default_format);
}

std::string Timestamp::local_str() const
{
    return this->local_format(default_format);
}

std::string Timestamp::zone_str() const
{
    // make_zoned to use %z
    return this->local_format(default_zone_format);
}

Timestamp Timestamp::floor_day() const
{
    return this->floor<date::days>();
}

Timestamp Timestamp::modulo_min(uint32_t minutes) const
{
    if (minutes > 60)
        throw std::invalid_argument(
            fmt::format("modulo minutes must be inferior or equal to 60 - was {}", minutes));

    const int now_minutes = this->clocktime().minute;
    const uint32_t interval = (now_minutes / minutes) * minutes;

    return this->floor<std::chrono::hours>() + time::minutes(interval);
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

std::optional<Timestamp> Timestamp::from_str(const std::string & date_str, std::string_view format)
{
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    std::chrono::system_clock::time_point tp;

    std::istringstream ss {date_str};
    ss.imbue(std::locale::classic());
    ss >> std::chrono::parse(format.data(), tp);

    if (ss.fail())
        return std::nullopt;

    return Timestamp {tp};
#else
    struct tm t = {};
    std::istringstream ss {date_str.data()};

    ss.imbue(std::locale::classic());
    ss >> std::get_time(&t, format.data());

    if (ss.fail())
        return std::nullopt;

    return Timestamp {time::local_tm(t)};
#endif
}

std::optional<Timestamp>
    Timestamp::from_str(const std::string & date_str, std::string_view format, const std::locale & loc)
{
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    std::chrono::system_clock::time_point tp;

    std::istringstream ss {date_str};
    ss.imbue(loc);
    ss >> std::chrono::parse(format.data(), tp);

    if (ss.fail())
        return std::nullopt;

    return Timestamp {tp};
#else
    struct tm t = {};
    std::istringstream ss {date_str.data()};

    ss.imbue(loc);
    ss >> std::get_time(&t, format.data());

    if (ss.fail())
        return std::nullopt;

    return Timestamp {time::local_tm(t)};
#endif
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

Timestamp::operator std::chrono::nanoseconds() const
{
    return std::chrono::nanoseconds(_nano);
}

Timestamp::operator std::chrono::microseconds() const
{
    return time::to_duration<std::chrono::microseconds>(_nano);
}

Timestamp::operator std::chrono::milliseconds() const
{
    return time::to_duration<std::chrono::milliseconds>(_nano);
}

Timestamp::operator std::chrono::seconds() const
{
    return time::to_duration<std::chrono::seconds>(_nano);
}

Timestamp::operator std::chrono::minutes() const
{
    return time::to_duration<std::chrono::minutes>(_nano);
}

Timestamp::operator std::chrono::hours() const
{
    return time::to_duration<std::chrono::hours>(_nano);
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

struct timespec Timestamp::ts() const
{
    return time::to_ts(_nano);
}

struct tm Timestamp::tm() const
{
    return time::to_tm(std::abs(_nano), false);
}

struct tm Timestamp::local_tm() const
{
    return time::to_tm(std::abs(_nano), true);
}

namespace time
{

void sleep_t(Timestamp ts)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(ts.nanoseconds()));
}

} // namespace time

} // namespace sihd::util
