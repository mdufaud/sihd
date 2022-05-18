#ifndef __SIHD_UTIL_TIMESTAMP_HPP__
# define __SIHD_UTIL_TIMESTAMP_HPP__

# include <sihd/util/time.hpp>
# include <chrono>
# include <string>

# define __TMP_TIMESTAMP_DURATION_COMPARISION_OPERATION__(OP) \
    template <typename T> \
    bool operator OP(std::chrono::duration<int64_t, T> duration) const \
    { \
        return _nano OP time::duration<T>(duration); \
    }

# define __TMP_TIMESTAMP_DURATION_ARITHMETIC_OPERATION__(OP) \
    template <typename T> \
    Timestamp operator OP(std::chrono::duration<int64_t, T> duration) \
    { \
        return Timestamp(_nano OP time::duration<T>(duration)); \
    }

# define __TMP_TIMESTAMP_DURATION_ASSIGN_OPERATION__(OP) \
    template <typename T> \
    Timestamp & operator OP(std::chrono::duration<int64_t, T> duration) \
    { \
        _nano OP time::duration<T>(duration); \
        return *this; \
    }

# define __TMP_TIMESTAMP_ASSIGN_OPERATION__(OP) \
    Timestamp & operator OP(time_t t) { _nano OP t; return *this; }

namespace sihd::util
{

class Timestamp
{
    public:
        Timestamp(time_t nano);
        explicit Timestamp(std::chrono::nanoseconds duration);
        explicit Timestamp(std::chrono::microseconds duration);
        explicit Timestamp(std::chrono::milliseconds duration);
        explicit Timestamp(std::chrono::seconds duration);
        explicit Timestamp(std::chrono::minutes duration);
        explicit Timestamp(std::chrono::hours duration);
        ~Timestamp();

        template <typename T>
        bool operator==(std::chrono::duration<int64_t, T> duration)
        {
            return _nano == time::duration<T>(duration);
        }

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

        __TMP_TIMESTAMP_ASSIGN_OPERATION__(+=);
        __TMP_TIMESTAMP_ASSIGN_OPERATION__(-=);
        __TMP_TIMESTAMP_ASSIGN_OPERATION__(*=);
        __TMP_TIMESTAMP_ASSIGN_OPERATION__(/=);
        __TMP_TIMESTAMP_ASSIGN_OPERATION__(%=);

        template <typename T>
        static Timestamp from(std::chrono::duration<int64_t, T> duration)
        {
            return Timestamp(time::duration<T>(duration));
        }

        operator time_t() const { return _nano; }
        operator std::chrono::nanoseconds() const { return std::chrono::nanoseconds(_nano); }
        operator std::chrono::microseconds() const { return time::to_duration<std::micro>(_nano); }
        operator std::chrono::milliseconds() const { return time::to_duration<std::milli>(_nano); }
        operator std::chrono::seconds() const { return time::to_duration<std::ratio<1>>(_nano); }
        operator std::chrono::minutes() const { return time::to_duration<std::ratio<60>>(_nano); }
        operator std::chrono::hours() const { return time::to_duration<std::ratio<3600>>(_nano); }

        template <typename T>
        std::chrono::duration<int64_t, T> duration() const
        {
            return time::to_duration<T>(_nano);
        }

        time_t nanoseconds() const { return _nano; }
        time_t microseconds() const { return time::to_microseconds(_nano); }
        time_t milliseconds() const { return time::to_milliseconds(_nano); }
        time_t seconds() const { return time::to_seconds(_nano); }
        time_t minutes() const { return time::to_minutes(_nano); }
        time_t hours() const { return time::to_hours(_nano); }
        time_t days() const { return time::to_days(_nano); }

        std::string gmtime_str(bool total_parenthesis = false, bool nano_resolution = false);
        std::string localtime_str(bool total_parenthesis = false, bool nano_resolution = false);

        time_t get() const { return _nano; }

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