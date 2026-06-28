#ifndef __SIHD_UTIL_TIMEBASE_HPP__
#define __SIHD_UTIL_TIMEBASE_HPP__

#include <chrono>
#include <ctime>

#include <sihd/util/time.hpp>
#include <sihd/util/traits.hpp>

#define __TMP_TIMEBASE_DURATION_COMPARISION_OPERATION__(OP)                                                  \
    template <traits::Duration Duration>                                                                     \
    constexpr bool operator OP(Duration duration) const                                                      \
    {                                                                                                        \
        return _nano OP time::duration<Duration>(duration);                                                  \
    }

#define __TMP_TIMEBASE_DURATION_ARITHMETIC_OPERATION__(OP)                                                   \
    template <traits::Duration Duration>                                                                     \
    constexpr Derived operator OP(Duration duration) const                                                   \
    {                                                                                                        \
        return Derived(_nano OP time::duration<Duration>(duration));                                         \
    }

#define __TMP_TIMEBASE_DURATION_ASSIGN_OPERATION__(OP)                                                       \
    template <traits::Duration Duration>                                                                     \
    constexpr Derived & operator OP(Duration duration)                                                       \
    {                                                                                                        \
        _nano OP time::duration<Duration>(duration);                                                         \
        return static_cast<Derived &>(*this);                                                                \
    }

#define __TMP_TIMEBASE_ASSIGN_OPERATION__(OP)                                                                \
    constexpr Derived & operator OP(time::UnixTime t)                                                        \
    {                                                                                                        \
        _nano OP t;                                                                                          \
        return static_cast<Derived &>(*this);                                                                \
    }

namespace sihd::util
{

// Shared nanosecond-backed core for Timestamp (absolute instant) and Duration (relative interval).
// CRTP base: arithmetic / assignment operators return the concrete Derived type.
template <typename Derived>
class TimeBase
{
    public:
        constexpr TimeBase(time::UnixTime nano = 0): _nano(nano) {};
        constexpr TimeBase(timespec ts): _nano(ts.tv_nsec + time::sec(ts.tv_sec)) {};

        template <typename T>
        constexpr TimeBase(std::chrono::time_point<T> timepoint): _nano(time::duration(timepoint.time_since_epoch())) {};

        template <traits::Duration Duration>
        constexpr TimeBase(Duration duration): _nano(time::duration(duration)) {};

        // std::chrono::duration templates
        __TMP_TIMEBASE_DURATION_COMPARISION_OPERATION__(==);
        __TMP_TIMEBASE_DURATION_COMPARISION_OPERATION__(!=);
        __TMP_TIMEBASE_DURATION_COMPARISION_OPERATION__(>=);
        __TMP_TIMEBASE_DURATION_COMPARISION_OPERATION__(<=);
        __TMP_TIMEBASE_DURATION_COMPARISION_OPERATION__(<);
        __TMP_TIMEBASE_DURATION_COMPARISION_OPERATION__(>);

        __TMP_TIMEBASE_DURATION_ARITHMETIC_OPERATION__(+);
        __TMP_TIMEBASE_DURATION_ARITHMETIC_OPERATION__(-);
        __TMP_TIMEBASE_DURATION_ARITHMETIC_OPERATION__(/);
        __TMP_TIMEBASE_DURATION_ARITHMETIC_OPERATION__(*);
        __TMP_TIMEBASE_DURATION_ARITHMETIC_OPERATION__(%);

        __TMP_TIMEBASE_DURATION_ASSIGN_OPERATION__(+=);
        __TMP_TIMEBASE_DURATION_ASSIGN_OPERATION__(-=);
        __TMP_TIMEBASE_DURATION_ASSIGN_OPERATION__(*=);
        __TMP_TIMEBASE_DURATION_ASSIGN_OPERATION__(/=);
        __TMP_TIMEBASE_DURATION_ASSIGN_OPERATION__(%=);

        // time_t assign
        __TMP_TIMEBASE_ASSIGN_OPERATION__(+=);
        __TMP_TIMEBASE_ASSIGN_OPERATION__(-=);
        __TMP_TIMEBASE_ASSIGN_OPERATION__(*=);
        __TMP_TIMEBASE_ASSIGN_OPERATION__(/=);
        __TMP_TIMEBASE_ASSIGN_OPERATION__(%=);

        // auto convert
        constexpr operator time::UnixTime() const { return _nano; }
        operator std::chrono::nanoseconds() const { return std::chrono::nanoseconds(_nano); }
        operator std::chrono::microseconds() const { return time::to_duration<std::chrono::microseconds>(_nano); }
        operator std::chrono::milliseconds() const { return time::to_duration<std::chrono::milliseconds>(_nano); }
        operator std::chrono::seconds() const { return time::to_duration<std::chrono::seconds>(_nano); }
        operator std::chrono::minutes() const { return time::to_duration<std::chrono::minutes>(_nano); }
        operator std::chrono::hours() const { return time::to_duration<std::chrono::hours>(_nano); }

        template <traits::Duration Duration>
        constexpr Duration duration() const
        {
            return time::to_duration<Duration>(_nano);
        }

        time::UnixTime nanoseconds() const { return _nano; }
        time::UnixTime microseconds() const { return time::to_microseconds(_nano); }
        time::UnixTime milliseconds() const { return time::to_milliseconds(_nano); }
        time::UnixTime seconds() const { return time::to_seconds(_nano); }
        time::UnixTime minutes() const { return time::to_minutes(_nano); }
        time::UnixTime hours() const { return time::to_hours(_nano); }
        time::UnixTime days() const { return time::to_days(_nano); }

        constexpr time::UnixTime get() const { return _nano; }

        template <typename Ratio>
        constexpr Derived floor() const
        {
            return Derived(time::floor<Ratio>(_nano));
        }

    protected:
        time::UnixTime _nano;
};

} // namespace sihd::util

#undef __TMP_TIMEBASE_DURATION_COMPARISION_OPERATION__
#undef __TMP_TIMEBASE_DURATION_ARITHMETIC_OPERATION__
#undef __TMP_TIMEBASE_DURATION_ASSIGN_OPERATION__
#undef __TMP_TIMEBASE_ASSIGN_OPERATION__

#endif
