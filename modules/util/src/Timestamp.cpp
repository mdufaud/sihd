#include <sys/time.h>

#include <limits>
#include <locale>
#include <sstream>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/str.hpp>

#if !defined(__cpp_lib_chrono) || __cpp_lib_chrono < 201907L
# include <iomanip>
#endif

namespace sihd::util
{
using enum ErrorCode;

namespace
{

std::unexpected<Error> date_parse_error(const std::string & date_str, const std::string & format)
{
    return std::unexpected(Error(invalid_argument, "could not parse date '{}' with format '{}'", date_str, format));
}

} // namespace

Timestamp::Timestamp(Calendar calendar)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = calendar.year - 1900;
    tm.tm_mon = calendar.month - 1;
    tm.tm_mday = calendar.day;
    tm.tm_isdst = -1;

    _nano = time::local_tm(tm);
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

    _nano = time::local_tm(tm);
    const auto offset = time::milliseconds(clocktime.millisecond);
    if ((offset > 0 && _nano > std::numeric_limits<int64_t>::max() - offset)
        || (offset < 0 && _nano < std::numeric_limits<int64_t>::min() - offset))
        throw std::out_of_range("calendar is outside the timestamp range");
    _nano += offset;
}

Timestamp Timestamp::now()
{
    return Timestamp(Clock::default_clock.now());
}

bool Timestamp::in_interval(Timestamp from, Duration offset) const
{
    return _nano >= from.get() && _nano <= (from.get() + offset.get());
}

std::string Timestamp::timeoffset_str(bool total_parenthesis, bool nano_resolution) const
{
    return str::timeoffset_str(Timestamp(_nano), total_parenthesis, nano_resolution);
}

std::string Timestamp::localtimeoffset_str(bool total_parenthesis, bool nano_resolution) const
{
    return str::localtimeoffset_str(Timestamp(_nano), total_parenthesis, nano_resolution);
}

std::string Timestamp::format(const std::string & format) const
{
    return str::format_time(*this, format);
}

std::string Timestamp::local_format(const std::string & format) const
{
    return str::format_localtime(*this, format);
}

std::string Timestamp::format(const std::string & format, const std::locale & loc) const
{
    return str::format_time(*this, format, loc);
}

std::string Timestamp::local_format(const std::string & format, const std::locale & loc) const
{
    return str::format_localtime(*this, format, loc);
}

std::string Timestamp::sec_str(const std::string & format) const
{
    return str::format_time(this->floor<std::chrono::seconds>(), format);
}

std::string Timestamp::local_sec_str(const std::string & format) const
{
    return str::format_localtime(this->floor<std::chrono::seconds>(), format);
}

std::string Timestamp::day_str(const std::string & format) const
{
    return str::format_time(this->floor<date::days>(), format);
}

std::string Timestamp::local_day_str(const std::string & format) const
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
    if (minutes == 0 || minutes > 60)
        throw std::invalid_argument(fmt::format("modulo minutes must be inferior or equal to 60 - was {}", minutes));

    const int now_minutes = this->clocktime().minute;
    const uint32_t interval = (now_minutes / minutes) * minutes;

    return this->floor<std::chrono::hours>() + Duration(time::minutes(interval));
}

Clocktime Timestamp::clocktime() const
{
    struct tm tm = time::to_tm(_nano, false);
    return {
        .hour = tm.tm_hour,
        .minute = tm.tm_min,
        .second = tm.tm_sec,
        .millisecond = time::clock_millisecond(_nano),
    };
}

Calendar Timestamp::calendar() const
{
    struct tm tm = time::to_tm(_nano, false);
    return {.day = tm.tm_mday, .month = tm.tm_mon + 1, .year = tm.tm_year + 1900};
}

Clocktime Timestamp::local_clocktime() const
{
    struct tm tm = time::to_tm(_nano, true);
    return {
        .hour = tm.tm_hour,
        .minute = tm.tm_min,
        .second = tm.tm_sec,
        .millisecond = time::clock_millisecond(_nano),
    };
}

Calendar Timestamp::local_calendar() const
{
    struct tm tm = time::to_tm(_nano, true);
    return {.day = tm.tm_mday, .month = tm.tm_mon + 1, .year = tm.tm_year + 1900};
}

Result<Timestamp> Timestamp::from_str(const std::string & date_str, const std::string & format)
{
    return Timestamp::from_str(date_str, format, std::locale::classic());
}

Result<Timestamp> Timestamp::from_str(const std::string & date_str, const std::string & format, const std::locale & loc)
{
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    std::chrono::sys_seconds seconds;
    std::chrono::sys_time<std::chrono::duration<uint64_t, std::nano>> parsed;

    std::istringstream ss {date_str};
    ss.imbue(loc);
    ss >> std::chrono::parse(format.c_str(), seconds);

    if (ss.fail() || seconds.time_since_epoch().count() < std::numeric_limits<int64_t>::min() / 1000000000 - 1
        || seconds.time_since_epoch().count() > std::numeric_limits<int64_t>::max() / 1000000000)
        return date_parse_error(date_str, format);

    ss.clear();
    ss.seekg(0);
    ss >> std::chrono::parse(format.c_str(), parsed);

    const std::chrono::sys_time<std::chrono::nanoseconds> tp {
        std::chrono::nanoseconds {static_cast<int64_t>(parsed.time_since_epoch().count())}};

    if (ss.fail() || std::chrono::floor<std::chrono::seconds>(tp) != seconds)
        return date_parse_error(date_str, format);

    return Timestamp {tp};
#else
    struct tm t = {};
    std::istringstream ss {date_str.data()};

    ss.imbue(loc);
    ss >> std::get_time(&t, format.c_str());

    if (ss.fail())
        return date_parse_error(date_str, format);

    try
    {
        return Timestamp {time::tm(t)};
    }
    catch (const std::out_of_range &)
    {
        return date_parse_error(date_str, format);
    }
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
    Calendar calendar = this->local_calendar();
    return time::is_leap_year(calendar.year);
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
    return time::to_tm(_nano, false);
}

struct tm Timestamp::local_tm() const
{
    return time::to_tm(_nano, true);
}

} // namespace sihd::util
