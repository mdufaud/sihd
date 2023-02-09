#ifndef __SIHD_UTIL_TIMESTAMP_HPP__
# define __SIHD_UTIL_TIMESTAMP_HPP__

# include <chrono>
# include <string>

# include <sihd/util/Time.hpp>


# define __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(OP) \
    template <typename T> \
    bool operator OP(std::chrono::duration<int64_t, T> duration) const \
    { \
        return _nano OP Time::duration<T>(duration); \
    }

# define __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__(OP) \
    template <typename T> \
    Timestamp operator OP(std::chrono::duration<int64_t, T> duration) \
    { \
        return Timestamp(_nano OP Time::duration<T>(duration)); \
    }

# define __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__(OP) \
    template <typename T> \
    Timestamp & operator OP(std::chrono::duration<int64_t, T> duration) \
    { \
        _nano OP Time::duration<T>(duration); \
        return *this; \
    }

# define __TMP_TIMESTAMP_ASSIGN_OPERATION__(OP) \
    Timestamp & operator OP(time_t t) { _nano OP t; return *this; }

namespace sihd::util
{

struct ClockTime
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
        Timestamp(ClockTime clocktime);
        Timestamp(Calendar calendar);
        Timestamp(Calendar calendar, ClockTime clocktime);
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
            return Timestamp(Time::duration<T>(duration));
        }

        static Timestamp now();

        bool is_leap_year() const;

        operator time_t() const { return _nano; }
        operator std::chrono::nanoseconds() const { return std::chrono::nanoseconds(_nano); }
        operator std::chrono::microseconds() const { return Time::to_duration<std::micro>(_nano); }
        operator std::chrono::milliseconds() const { return Time::to_duration<std::milli>(_nano); }
        operator std::chrono::seconds() const { return Time::to_duration<std::ratio<1>>(_nano); }
        operator std::chrono::minutes() const { return Time::to_duration<std::ratio<60>>(_nano); }
        operator std::chrono::hours() const { return Time::to_duration<std::ratio<3600>>(_nano); }

        // this >= from && this <= (from + offset)
        bool in_interval(Timestamp from, Timestamp offset) const;

        template <typename T>
        std::chrono::duration<int64_t, T> duration() const
        {
            return Time::to_duration<T>(_nano);
        }

        time_t nanoseconds() const { return _nano; }
        time_t microseconds() const { return Time::to_microseconds(_nano); }
        time_t milliseconds() const { return Time::to_milliseconds(_nano); }
        time_t seconds() const { return Time::to_seconds(_nano); }
        time_t minutes() const { return Time::to_minutes(_nano); }
        time_t hours() const { return Time::to_hours(_nano); }
        time_t days() const { return Time::to_days(_nano); }

        std::string timeoffset_str(bool total_parenthesis = false, bool nano_resolution = false) const;
        std::string localtimeoffset_str(bool total_parenthesis = false, bool nano_resolution = false) const;
        // format strftime -> "%Y-%m-%d %H:%M:%S"
        std::string format(std::string_view format = "%d/%m/%Y %H:%M:%S") const;
        std::string local_format(std::string_view format = "%d/%m/%Y %H:%M:%S") const;

        time_t get() const { return _nano; }
        ClockTime clocktime() const;
        Calendar calendar() const;
        ClockTime local_clocktime() const;
        Calendar local_calendar() const;

    protected:

    private:
        time_t _nano;
};

}

# undef __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__
# undef __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__
# undef __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__
# undef __TMP_TIMESTAMP_ASSIGN_OPERATION__

#endif
