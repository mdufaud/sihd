#ifndef __SIHD_UTIL_TIMESTAMP_HPP__
#define __SIHD_UTIL_TIMESTAMP_HPP__

#include <optional>
#include <string>
#include <string_view>

#include <sihd/util/time.hpp>

#define __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(OP)                                                           \
    template <traits::Duration Duration>                                                                               \
    constexpr bool operator OP(Duration duration) const                                                                \
    {                                                                                                                  \
        return _nano OP time::duration<Duration>(duration);                                                            \
    }

#define __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__(OP)                                                            \
    template <traits::Duration Duration>                                                                               \
    constexpr Timestamp operator OP(Duration duration)                                                                 \
    {                                                                                                                  \
        return Timestamp(_nano OP time::duration<Duration>(duration));                                                 \
    }

#define __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__(OP)                                                                \
    template <traits::Duration Duration>                                                                               \
    constexpr Timestamp & operator OP(Duration duration)                                                               \
    {                                                                                                                  \
        _nano OP time::duration<Duration>(duration);                                                                   \
        return *this;                                                                                                  \
    }

#define __TMP_TIMESTAMP_ASSIGN_OPERATION__(OP)                                                                         \
    constexpr Timestamp & operator OP(time_t t)                                                                        \
    {                                                                                                                  \
        _nano OP t;                                                                                                    \
        return *this;                                                                                                  \
    }

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

class Timestamp
{
    public:
        static constexpr const char *default_format = "%Y/%m/%d %H:%M:%S";
        static constexpr const char *default_day_format = "%Y-%m-%d";
        static constexpr const char *default_zone_format = "%FT%T%z";

        constexpr Timestamp(time_t nano = 0): _nano(nano) {};
        constexpr Timestamp(timespec ts): _nano(ts.tv_nsec + time::sec(ts.tv_sec)) {};
        constexpr Timestamp(Clocktime clocktime): _nano(0)
        {
            _nano = time::hours(clocktime.hour);
            _nano += time::minutes(clocktime.minute);
            _nano += time::seconds(clocktime.second);
            _nano += time::milliseconds(clocktime.millisecond);
        };

        template <typename T>
        constexpr Timestamp(std::chrono::time_point<T> timepoint): _nano(timepoint.time_since_epoch().count()) {};

        template <traits::Duration Duration>
        constexpr Timestamp(Duration duration): _nano(time::duration(duration)) {};

        Timestamp(Calendar calendar);
        Timestamp(Calendar calendar, Clocktime clocktime);

        // std::chrono::duration templates
        __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(==);
        __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(!=);
        __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(>=);
        __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(<=);
        __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(<);
        __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(>);

        __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__(+);
        __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__(-);
        __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__(/);
        __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__(*);
        __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__(%);

        __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__(+=);
        __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__(-=);
        __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__(*=);
        __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__(/=);
        __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__(%=);

        // time_t assign
        __TMP_TIMESTAMP_ASSIGN_OPERATION__(+=);
        __TMP_TIMESTAMP_ASSIGN_OPERATION__(-=);
        __TMP_TIMESTAMP_ASSIGN_OPERATION__(*=);
        __TMP_TIMESTAMP_ASSIGN_OPERATION__(/=);
        __TMP_TIMESTAMP_ASSIGN_OPERATION__(%=);

        static Timestamp now();
        static std::optional<Timestamp> from_str(std::string_view date_str, std::string_view format = default_format);

        // auto convert
        constexpr operator time_t() const { return _nano; }
        operator std::chrono::nanoseconds() const;
        operator std::chrono::microseconds() const;
        operator std::chrono::milliseconds() const;
        operator std::chrono::seconds() const;
        operator std::chrono::minutes() const;
        operator std::chrono::hours() const;

        // this >= from && this <= (from + offset)
        bool in_interval(Timestamp from, Timestamp offset) const;

        template <traits::Duration Duration>
        constexpr Duration duration() const
        {
            return time::to_duration<Duration>(_nano);
        }

        time_t nanoseconds() const;
        time_t microseconds() const;
        time_t milliseconds() const;
        time_t seconds() const;
        time_t minutes() const;
        time_t hours() const;
        time_t days() const;

        struct timeval tv() const;
        struct tm tm() const;
        struct tm local_tm() const;

        std::string timeoffset_str(bool total_parenthesis = false, bool nano_resolution = false) const;
        std::string localtimeoffset_str(bool total_parenthesis = false, bool nano_resolution = false) const;

        // UTC format
        std::string format(std::string_view format) const;
        std::string local_format(std::string_view format) const;

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

        template <typename Ratio>
        constexpr Timestamp floor() const
        {
            return Timestamp(time::floor<Ratio>(_nano));
        }
        Timestamp floor_day() const;
        Timestamp modulo_min(uint32_t minutes) const;

        constexpr time_t get() const { return _nano; }

        Clocktime clocktime() const;
        Calendar calendar() const;
        Clocktime local_clocktime() const;
        Calendar local_calendar() const;

    protected:

    private:
        time_t _nano;
};

namespace time
{

void sleep_t(Timestamp ts);

} // namespace time

} // namespace sihd::util

#undef __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__
#undef __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__
#undef __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__
#undef __TMP_TIMESTAMP_ASSIGN_OPERATION__

#endif
