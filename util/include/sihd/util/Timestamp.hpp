#ifndef __SIHD_UTIL_TIMESTAMP_HPP__
#define __SIHD_UTIL_TIMESTAMP_HPP__

#include <string>

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

        template <typename T>
        static Timestamp from(std::chrono::time_point<T> timepoint)
        {
            return Timestamp(timepoint.time_since_epoch().count());
        }

        template <typename T>
        static Timestamp from(std::chrono::duration<int64_t, T> duration)
        {
            return Timestamp(time::duration<T>(duration));
        }

        static Timestamp now();

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

        template <typename T>
        std::chrono::duration<int64_t, T> duration() const
        {
            return time::to_duration<T>(_nano);
        }

        time_t nanoseconds() const;
        time_t microseconds() const;
        time_t milliseconds() const;
        time_t seconds() const;
        time_t minutes() const;
        time_t hours() const;
        time_t days() const;

        struct timeval tv() const;

        std::string timeoffset_str(bool total_parenthesis = false, bool nano_resolution = false) const;
        std::string localtimeoffset_str(bool total_parenthesis = false, bool nano_resolution = false) const;
        // format strftime -> "%Y-%m-%d %H:%M:%S"
        std::string format(std::string_view format = "%d/%m/%Y %H:%M:%S") const;
        std::string local_format(std::string_view format = "%d/%m/%Y %H:%M:%S") const;

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
