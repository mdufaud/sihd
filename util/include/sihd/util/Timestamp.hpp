#ifndef __SIHD_UTIL_TIMESTAMP_HPP__
#define __SIHD_UTIL_TIMESTAMP_HPP__

#include <optional>
#include <string>
#include <string_view>

#include <sihd/util/time.hpp>

#define __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(OP)                                                           \
 template <typename T>                                                                                                 \
 bool operator OP(std::chrono::duration<int64_t, T> duration) const                                                    \
 {                                                                                                                     \
  return _nano OP time::duration<T>(duration);                                                                         \
 }

#define __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__(OP)                                                            \
 template <typename T>                                                                                                 \
 Timestamp operator OP(std::chrono::duration<int64_t, T> duration)                                                     \
 {                                                                                                                     \
  return Timestamp(_nano OP time::duration<T>(duration));                                                              \
 }

#define __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__(OP)                                                                \
 template <typename T>                                                                                                 \
 Timestamp & operator OP(std::chrono::duration<int64_t, T> duration)                                                   \
 {                                                                                                                     \
  _nano OP time::duration<T>(duration);                                                                                \
  return *this;                                                                                                        \
 }

#define __TMP_TIMESTAMP_ASSIGN_OPERATION__(OP)                                                                         \
 Timestamp & operator OP(time_t t)                                                                                     \
 {                                                                                                                     \
  _nano OP t;                                                                                                          \
  return *this;                                                                                                        \
 }

namespace sihd::util
{

namespace date
{

using days = std::chrono::duration<int64_t, std::ratio<86400>>;
using weeks = std::chrono::duration<int64_t, std::ratio<604800>>;
using months = std::chrono::duration<int64_t, std::ratio<2629746>>;
using years = std::chrono::duration<int64_t, std::ratio<31556952>>;

} // namespace date

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
        int day = 0;
        // 1 - 12
        int month = 0;
        // 0 - X
        int year = 0;

        std::string str() const;
};

inline constexpr const char *default_format = "%Y/%m/%d %H:%M:%S";
inline constexpr const char *default_day_format = "%Y-%m-%d";
inline constexpr const char *default_zone_format = "%FT%T%z";

class Timestamp
{
    public:
        Timestamp(time_t nano);
        Timestamp(Clocktime clocktime);
        Timestamp(Calendar calendar);
        Timestamp(Calendar calendar, Clocktime clocktime);
        Timestamp(std::chrono::nanoseconds duration);
        Timestamp(std::chrono::microseconds duration);
        Timestamp(std::chrono::milliseconds duration);
        Timestamp(std::chrono::seconds duration);
        Timestamp(std::chrono::minutes duration);
        Timestamp(std::chrono::hours duration);

        template <typename T>
        Timestamp(std::chrono::time_point<T> timepoint)
        {
            _nano = timepoint.time_since_epoch().count();
        }

        template <typename T>
        Timestamp(std::chrono::duration<int64_t, T> duration)
        {
            _nano = time::duration<T>(duration);
        }

        ~Timestamp();

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

        bool is_leap_year() const;

        operator time_t() const;
        operator std::chrono::nanoseconds() const;
        operator std::chrono::microseconds() const;
        operator std::chrono::milliseconds() const;
        operator std::chrono::seconds() const;
        operator std::chrono::minutes() const;
        operator std::chrono::hours() const;

        // this >= from && this <= (from + offset)
        bool in_interval(Timestamp from, Timestamp offset) const;

        template <typename Ratio = std::nano>
        std::chrono::duration<int64_t, Ratio> duration() const
        {
            return time::to_duration<Ratio>(_nano);
        }

        template <typename Clock = std::chrono::system_clock>
        std::chrono::time_point<Clock> timepoint() const
        {
            return std::chrono::time_point<Clock>(this->duration<std::nano>());
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

        std::string format(std::string_view format) const;
        std::string local_format(std::string_view format) const;

        // uses default_format
        std::string str() const;
        std::string local_str() const;

        // uses default_zone_format
        std::string zone_str() const;

        // floored to second value
        std::string sec_str(std::string_view format = default_format) const;
        // floored to second value
        std::string local_sec_str(std::string_view format = default_format) const;

        // used default_day_format + floored value
        std::string day_str(std::string_view format = default_day_format) const;
        // default_day_format + floored value
        std::string local_day_str(std::string_view format = default_day_format) const;

        template <typename T>
        Timestamp floor() const
        {
            return Timestamp(std::chrono::floor<T>(this->timepoint()).time_since_epoch().count());
        }
        Timestamp floor_day() const;
        Timestamp modulo_min(uint32_t minutes) const;

        time_t get() const { return _nano; }

        Clocktime clocktime() const;
        Calendar calendar() const;
        Clocktime local_clocktime() const;
        Calendar local_calendar() const;

    protected:

    private:
        time_t _nano;
};

} // namespace sihd::util

#undef __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__
#undef __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__
#undef __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__
#undef __TMP_TIMESTAMP_ASSIGN_OPERATION__

#endif
