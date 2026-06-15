#ifndef __SIHD_UTIL_TIMESTAMP_HPP__
#define __SIHD_UTIL_TIMESTAMP_HPP__

#include <concepts>
#include <optional>
#include <string>
#include <string_view>

#include <sihd/util/Duration.hpp>
#include <sihd/util/TimeBase.hpp>
#include <sihd/util/time.hpp>

namespace sihd::util
{

struct Clocktime
{
        // 0 - 23
        int hour = 0;
        // 0 - 59
        int minute = 0;
        // 0 - 59
        int second = 0;
        // 0 - 999
        int millisecond = 0;

        std::string str() const;
};

struct Calendar
{
        // month day -> 1 - 31
        int day = 1;
        // 1 - 12
        int month = 1;
        // 0 - X
        int year = 0;

        std::string str() const;
};

class Timestamp : public TimeBase<Timestamp>
{
    public:
        static constexpr const char *default_format = "%Y/%m/%d %H:%M:%S";
        static constexpr const char *default_day_format = "%Y-%m-%d";
        static constexpr const char *default_zone_format = "%FT%T%z";

        using TimeBase<Timestamp>::TimeBase;

        constexpr Timestamp(Clocktime clocktime): TimeBase<Timestamp>(0)
        {
            _nano = time::hours(clocktime.hour);
            _nano += time::minutes(clocktime.minute);
            _nano += time::seconds(clocktime.second);
            _nano += time::milliseconds(clocktime.millisecond);
        };

        Timestamp(Calendar calendar);
        Timestamp(Calendar calendar, Clocktime clocktime);

        static Timestamp now();
        static std::optional<Timestamp> from_str(const std::string & date_str,
                                                 std::string_view format = default_format);
        // Parse with explicit locale (default: C locale for deterministic behavior)
        static std::optional<Timestamp>
            from_str(const std::string & date_str, std::string_view format, const std::locale & loc);

        // this >= from && this <= (from + offset)
        bool in_interval(Timestamp from, Duration offset) const;

        struct timespec ts() const;
        struct timeval tv() const;
        struct tm tm() const;
        struct tm local_tm() const;

        std::string timeoffset_str(bool total_parenthesis = false, bool nano_resolution = false) const;
        std::string localtimeoffset_str(bool total_parenthesis = false, bool nano_resolution = false) const;

        // UTC format
        std::string format(std::string_view format) const;
        std::string local_format(std::string_view format) const;
        // UTC format with explicit locale (default: C locale)
        std::string format(std::string_view format, const std::locale & loc) const;
        std::string local_format(std::string_view format, const std::locale & loc) const;

        // uses UTC with default_format
        std::string str() const;
        std::string local_str() const;

        // uses UTC with default_zone_format
        std::string zone_str() const;

        // UTC floored to second value
        std::string sec_str(std::string_view format = default_format) const;
        // floored to second value
        std::string local_sec_str(std::string_view format = default_format) const;

        // UTC used default_day_format + floored value
        std::string day_str(std::string_view format = default_day_format) const;
        // default_day_format + floored value
        std::string local_day_str(std::string_view format = default_day_format) const;

        bool is_leap_year() const;

        Timestamp floor_day() const;
        Timestamp modulo_min(uint32_t minutes) const;

        Clocktime clocktime() const;
        Calendar calendar() const;
        Clocktime local_clocktime() const;
        Calendar local_calendar() const;
};

// Strict time algebra (chrono-like). The cross-type operators are constrained to the *exact*
// peer type via std::same_as: template deduction ignores implicit conversions, so chrono/UnixTime
// operands fall through to the TimeBase chrono operators (back-compat) instead of being absorbed here.
//
//   Timestamp - Timestamp -> Duration (elapsed)   Timestamp +/- Duration -> Timestamp
//   Duration +/- Duration -> Duration             Duration * / scalar -> Duration
//   Timestamp + Timestamp is intentionally ill-formed.

template <std::same_as<Duration> D>
constexpr Timestamp operator+(Timestamp ts, D offset)
{
    return Timestamp(ts.get() + offset.get());
}
template <std::same_as<Duration> D>
constexpr Timestamp operator+(D offset, Timestamp ts)
{
    return Timestamp(ts.get() + offset.get());
}
template <std::same_as<Duration> D>
constexpr Timestamp operator-(Timestamp ts, D offset)
{
    return Timestamp(ts.get() - offset.get());
}
template <std::same_as<Timestamp> T>
constexpr Duration operator-(Timestamp lhs, T rhs)
{
    return Duration(lhs.get() - rhs.get());
}
template <std::same_as<Timestamp> T>
Timestamp operator+(Timestamp lhs, T rhs) = delete;

template <std::same_as<Duration> D>
constexpr Duration operator+(Duration lhs, D rhs)
{
    return Duration(lhs.get() + rhs.get());
}
template <std::same_as<Duration> D>
constexpr Duration operator-(Duration lhs, D rhs)
{
    return Duration(lhs.get() - rhs.get());
}
template <traits::Arithmetic T>
constexpr Duration operator*(Duration d, T scalar)
{
    return Duration(static_cast<time::UnixTime>(d.get() * scalar));
}
template <traits::Arithmetic T>
constexpr Duration operator/(Duration d, T scalar)
{
    return Duration(static_cast<time::UnixTime>(d.get() / scalar));
}
template <std::same_as<Duration> D>
constexpr Duration operator%(Duration lhs, D rhs)
{
    return Duration(lhs.get() % rhs.get());
}

namespace time
{

void sleep_t(Duration duration);

} // namespace time

} // namespace sihd::util

#endif
