#include <sys/time.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include <sihd/util/build.hpp>
#include <sihd/util/time.hpp>

#if !defined(__SIHD_WINDOWS__)
extern char *tzname[2]; // 0 is standard and 1 is daylight saving
extern long timezone;
#else
# include <timezoneapi.h>
# include <windows.h>
#endif

namespace sihd::util::time
{

namespace
{

std::lldiv_t split_time(UnixTime value, UnixTime units)
{
    auto parts = std::lldiv(value, units);
    if (parts.rem < 0)
    {
        --parts.quot;
        parts.rem += units;
    }
    return parts;
}

template <typename Seconds>
Seconds platform_seconds(UnixTime seconds)
{
    if (!std::in_range<Seconds>(seconds))
        throw std::out_of_range("seconds are outside the platform range");
    return static_cast<Seconds>(seconds);
}

UnixTime compose_time(UnixTime seconds, UnixTime nanoseconds)
{
    if (seconds < 0 && nanoseconds > 0)
    {
        ++seconds;
        nanoseconds -= 1000000000;
    }
    if (seconds < std::numeric_limits<UnixTime>::min() / 1000000000
        || seconds > std::numeric_limits<UnixTime>::max() / 1000000000)
        throw std::out_of_range("seconds are outside the timestamp range");
    const auto nano = time::seconds(seconds);
    if ((nanoseconds > 0 && nano > std::numeric_limits<UnixTime>::max() - nanoseconds)
        || (nanoseconds < 0 && nano < std::numeric_limits<UnixTime>::min() - nanoseconds))
        throw std::out_of_range("time is outside the timestamp range");
    return nano + nanoseconds;
}

#if !defined(__SIHD_WINDOWS__)
void initialize_timezone()
{
    static std::once_flag initialized;
    std::call_once(initialized, tzset);
}
#endif

struct tm utc_tm(UnixTime seconds)
{
    const std::chrono::sys_seconds point {std::chrono::seconds {seconds}};
    const auto day = std::chrono::floor<std::chrono::days>(point);
    const std::chrono::year_month_day date {day};
    const std::chrono::hh_mm_ss clock {point - day};
    struct tm result {};
#if !defined(__SIHD_WINDOWS__)
    const time_t epoch = 0;
    ::gmtime_r(&epoch, &result);
#endif
    result.tm_year = static_cast<int>(date.year()) - 1900;
    result.tm_mon = static_cast<unsigned>(date.month()) - 1;
    result.tm_mday = static_cast<unsigned>(date.day());
    result.tm_hour = clock.hours().count();
    result.tm_min = clock.minutes().count();
    result.tm_sec = clock.seconds().count();
    result.tm_wday = std::chrono::weekday {day}.c_encoding();
    result.tm_yday = (day - std::chrono::sys_days {date.year() / std::chrono::January / 1}).count();
    return result;
}

UnixTime calendar_to_unix(struct tm & calendar, time_t (*convert)(struct tm *))
{
    errno = 0;
    const time_t seconds = convert(&calendar);
    if (seconds == time_t(-1) && errno != 0)
        throw std::out_of_range("calendar is outside the timestamp range");
    return compose_time(seconds, 0);
}

} // namespace

time_t get_timezone()
{
#if defined(__SIHD_WINDOWS__)
    TIME_ZONE_INFORMATION tzi;

    DWORD dwRet = GetTimeZoneInformation(&tzi);
    if (dwRet != TIME_ZONE_ID_INVALID)
        return tzi.Bias * 60;
    return 0;
#else
    initialize_timezone();
    return timezone;
#endif
}

std::string get_timezone_name()
{
#if defined(__SIHD_WINDOWS__)
    TIME_ZONE_INFORMATION tzi;

    DWORD dwRet = GetTimeZoneInformation(&tzi);
    const wchar_t *wname = nullptr;
    if (dwRet == TIME_ZONE_ID_STANDARD)
        wname = tzi.StandardName;
    else if (dwRet == TIME_ZONE_ID_DAYLIGHT)
        wname = tzi.DaylightName;
    else
        return "";

    int size = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return "";

    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, &result[0], size, nullptr, nullptr);
    return result;
#else
    initialize_timezone();
    return tzname[0];
#endif
}

void nsleep(UnixTime nano)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
}

void usleep(UnixTime micro)
{
    std::this_thread::sleep_for(std::chrono::microseconds(micro));
}

void msleep(UnixTime milli)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milli));
}

void sleep(time_t sec)
{
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

double tv_to_double(const struct timeval & tv)
{
    return (double)tv.tv_sec + (tv.tv_usec / 1E6);
}

double nano_tv_to_double(const struct timeval & tv)
{
    return (double)tv.tv_sec + (tv.tv_usec / 1E9);
}

double to_double(UnixTime nano)
{
    return nano / 1E9;
}

double to_double_milliseconds(UnixTime nano)
{
    return nano / 1E6;
}

double to_freq(UnixTime nano)
{
    if (nano <= 0)
        return 0;
    return (1 / (double)(nano / 1E9));
}

struct timeval double_to_tv(double time)
{
    struct timeval ret;

    ret.tv_sec = (time_t)time;
    ret.tv_usec = (time - (time_t)time) * 1E6;
    return ret;
}

struct timeval double_to_nano_tv(double time)
{
    struct timeval ret;

    ret.tv_sec = (time_t)time;
    ret.tv_usec = (time - (time_t)time) * 1E9;
    return ret;
}

struct timeval milli_to_tv(UnixTime milliseconds)
{
    struct timeval ret;
    const auto parts = split_time(milliseconds, 1000);
    ret.tv_sec = platform_seconds<decltype(ret.tv_sec)>(parts.quot);
    ret.tv_usec = parts.rem * 1000;
    return ret;
}

struct timeval to_tv(UnixTime micro)
{
    struct timeval ret;

    const auto parts = split_time(micro, 1000000);
    ret.tv_sec = platform_seconds<decltype(ret.tv_sec)>(parts.quot);
    ret.tv_usec = parts.rem;
    return ret;
}

struct timeval to_nano_tv(UnixTime nano)
{
    struct timeval ret;

    const auto parts = split_time(nano, 1000000000);
    ret.tv_sec = platform_seconds<decltype(ret.tv_sec)>(parts.quot);
    ret.tv_usec = parts.rem;
    return ret;
}

struct tm to_tm(UnixTime nano, bool localtime)
{
    const UnixTime seconds = split_time(nano, 1000000000).quot;
    if (!std::in_range<time_t>(seconds))
    {
        if (!localtime)
            return utc_tm(seconds);
        throw std::out_of_range("local calendar is outside the platform range");
    }
    const time_t sec = static_cast<time_t>(seconds);
#if defined(__SIHD_WINDOWS__)
    if (!localtime)
        return utc_tm(sec);
    if (sec < 0)
        throw std::out_of_range("local calendar is unavailable before 1970 on Windows");
    static std::mutex unsafe_c_mutex;
    std::lock_guard lock(unsafe_c_mutex);
    const struct tm *result = ::localtime(&sec);
    if (result == nullptr)
        throw std::out_of_range("local calendar is outside the platform range");
    return *result;
#else
    struct tm result;
    const struct tm *converted;
    if (localtime)
    {
        initialize_timezone();
        converted = ::localtime_r(&sec, &result);
    }
    else
        converted = ::gmtime_r(&sec, &result);
    if (converted == nullptr)
        throw std::out_of_range("calendar is outside the platform range");
    return result;
#endif
}

UnixTime to_micro(UnixTime nano)
{
    return nano / 1000;
}

UnixTime to_milli(UnixTime nano)
{
    return nano / 1000000;
}

UnixTime to_sec(UnixTime nano)
{
    return nano / 1000000000;
}

UnixTime to_min(UnixTime nano)
{
    return to_sec(nano) / 60;
}

UnixTime to_hours(UnixTime nano)
{
    return to_min(nano) / 60;
}

UnixTime to_days(UnixTime nano)
{
    return to_hours(nano) / 24;
}

int clock_hour(UnixTime nano)
{
    return (int)(split_time(nano, days(1)).rem / hours(1));
}

int clock_minute(UnixTime nano)
{
    return (int)(split_time(nano, hours(1)).rem / min(1));
}

int clock_second(UnixTime nano)
{
    return (int)(split_time(nano, min(1)).rem / sec(1));
}

int clock_millisecond(UnixTime nano)
{
    return (int)(split_time(nano, sec(1)).rem / milli(1));
}

int clock_microsecond(UnixTime nano)
{
    return (int)(split_time(nano, sec(1)).rem / micro(1));
}

UnixTime from_double(double sec_milli)
{
    time_t sec = (time_t)sec_milli;
    UnixTime nano = (double)(sec_milli - sec) * 1E9;
    return time::sec(sec) + nano;
}

UnixTime from_double_milliseconds(double milli_micro)
{
    time_t milliseconds = (time_t)milli_micro;
    UnixTime nano = (double)(milli_micro - milliseconds) * 1E6;
    return time::milliseconds(milliseconds) + nano;
}

UnixTime tv(const struct timeval & tv)
{
    return compose_time(tv.tv_sec, static_cast<UnixTime>(tv.tv_usec) * 1000);
}

UnixTime nano_tv(const struct timeval & tv)
{
    return compose_time(tv.tv_sec, tv.tv_usec);
}

UnixTime tm(struct tm & tm)
{
#if defined(__SIHD_WINDOWS__)
    return calendar_to_unix(tm, _mkgmtime);
#else
    return calendar_to_unix(tm, timegm);
#endif
}

UnixTime local_tm(struct tm & tm)
{
    return calendar_to_unix(tm, mktime);
}

bool is_leap_year(int year)
{
    return year % 4 == 0 && (year % 400 == 0 || year % 100 != 0);
}

UnixTime ts(const struct timespec & ts)
{
    return compose_time(ts.tv_sec, ts.tv_nsec);
}

struct timespec to_ts(UnixTime nano)
{
    struct timespec ts;
    const auto parts = split_time(nano, 1000000000);
    ts.tv_sec = platform_seconds<decltype(ts.tv_sec)>(parts.quot);
    ts.tv_nsec = parts.rem;
    return ts;
}

} // namespace sihd::util::time
