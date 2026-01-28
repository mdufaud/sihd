#include <sys/time.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>

#include <sihd/util/platform.hpp>
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

#if !defined(__SIHD_WINDOWS__)
std::atomic<bool> g_tz_is_set = false;
#endif
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
    if (g_tz_is_set.exchange(true))
        tzset();
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

    // Convert wide string to UTF-8 using Windows API (std::wstring_convert is deprecated)
    int size = WideCharToMultiByte(CP_UTF8, 0, wname, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return "";

    std::string result(size - 1, '\0'); // size includes null terminator
    WideCharToMultiByte(CP_UTF8, 0, wname, -1, &result[0], size, nullptr, nullptr);
    return result;
#else
    if (g_tz_is_set.exchange(true))
        tzset();
    return tzname[0];
#endif
}

void nsleep(time_t nano)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(nano));
}

void usleep(time_t micro)
{
    std::this_thread::sleep_for(std::chrono::microseconds(micro));
}

void msleep(time_t milli)
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

double to_double(time_t nano)
{
    return nano / 1E9;
}

double to_double_milliseconds(time_t nano)
{
    return nano / 1E6;
}

double to_freq(time_t nano)
{
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

struct timeval milli_to_tv(time_t milliseconds)
{
    struct timeval ret;

    ret.tv_sec = milliseconds / 1E3;
    ret.tv_usec = (milliseconds % (int)1E3) * 1E3;
    return ret;
}

struct timeval to_tv(time_t micro)
{
    struct timeval ret;

    ret.tv_sec = micro / 1E6;
    ret.tv_usec = micro % (int)1E6;
    return ret;
}

struct timeval to_nano_tv(time_t nano)
{
    struct timeval ret;

    ret.tv_sec = nano / 1E9;
    ret.tv_usec = nano % (int)1E9;
    return ret;
}

struct tm to_tm(time_t nano, bool localtime)
{
    time_t sec = nano / 1E9;
#if defined(__SIHD_WINDOWS__)
    static std::mutex unsafe_c_mutex;
    std::lock_guard l(unsafe_c_mutex);
    // make a copy
    return localtime ? *::localtime(&sec) : *::gmtime(&sec);
#else
    // localtime/gmtime_r is non rentrant
    struct tm result;
    if (localtime)
    {
        if (g_tz_is_set.exchange(true))
            tzset();
        ::localtime_r(&sec, &result);
    }
    else
        ::gmtime_r(&sec, &result);
    return result;
#endif
}

time_t to_micro(time_t nano)
{
    return nano / 1E3;
}

time_t to_milli(time_t nano)
{
    return nano / 1E6;
}

time_t to_sec(time_t nano)
{
    return nano / 1E9;
}

time_t to_min(time_t nano)
{
    return to_sec(nano) / 60;
}

time_t to_hours(time_t nano)
{
    return to_min(nano) / 60;
}

time_t to_days(time_t nano)
{
    return to_hours(nano) / 24;
}

time_t from_double(double sec_milli)
{
    time_t sec = (time_t)sec_milli;
    time_t nano = (double)(sec_milli - sec) * 1E9;
    return time::sec(sec) + nano;
}

time_t from_double_milliseconds(double milli_micro)
{
    time_t milliseconds = (time_t)milli_micro;
    time_t nano = (double)(milli_micro - milliseconds) * 1E6;
    return time::milliseconds(milliseconds) + nano;
}

time_t tv(const struct timeval & tv)
{
    return (tv.tv_sec * 1E9) + (tv.tv_usec * 1E3);
}

time_t nano_tv(const struct timeval & tv)
{
    return (tv.tv_sec * 1E9) + tv.tv_usec;
}

time_t tm(struct tm & tm)
{
    return mktime(&tm) * 1E9;
}

time_t local_tm(struct tm & tm)
{
    return (mktime(&tm) - get_timezone()) * 1E9;
}

bool is_leap_year(int year)
{
    return year % 4 == 0 && (year % 400 == 0 || year % 100 != 0);
}

time_t ts(const struct timespec & ts)
{
    return (ts.tv_sec * 1E9) + ts.tv_nsec;
}

struct timespec to_ts(time_t nano)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(nano / 1E9);
    ts.tv_nsec = (time_t)(nano % (int)1E9);
    return ts;
}

} // namespace sihd::util::time
