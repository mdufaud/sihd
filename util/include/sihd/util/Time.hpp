#ifndef __SIHD_UTIL_TIME_HPP__
# define __SIHD_UTIL_TIME_HPP__

# include <sys/time.h>
# include <time.h>

# include <chrono>
# include <mutex>

# include <sihd/util/platform.hpp>

namespace sihd::util
{

class Time
{
    public:
        Time() = delete;

        static time_t get_timezone();
        static std::string get_tzname();

        static void nsleep(time_t nano);
        static void usleep(time_t micro);
        static void msleep(time_t milli);
        static void sleep(time_t sec);

        // tv (micro resolution) -> seconds,milliseconds
        static double tv_to_double(const struct timeval & tv);
        // tv (nano resolution) -> seconds,milliseconds
        static double nano_tv_to_double(const struct timeval & tv);

        // seconds,milliseconds -> timeval
        static struct timeval double_to_tv(double time);
        // seconds,milliseconds -> timeval
        static struct timeval double_to_nano_tv(double time);
        // milliseconds -> timeval (micro resolution)
        static struct timeval milli_to_tv(time_t milliseconds);
        static constexpr auto ms_to_tv = Time::milli_to_tv;
        static constexpr auto milliseconds_to_tv = Time::milli_to_tv;
        // nano -> timeval (micro resolution)
        static struct timeval to_tv(time_t micro);
        // nano -> timeval (nano resolution)
        static struct timeval to_nano_tv(time_t nano);

        /*
        * nano -> statically allocated nano, other call to localtime/gmtime erase previous result
        * may be nullptr if year cannot be contained
        */
        static struct tm to_tm(time_t nano, bool localtime = false);

        // nano -> micro
        static time_t to_micro(time_t nano);
        static constexpr auto to_us = Time::to_micro;
        static constexpr auto to_microseconds = Time::to_micro;
        // nano -> milli
        static time_t to_milli(time_t nano);
        static constexpr auto to_ms = Time::to_milli;
        static constexpr auto to_milliseconds = Time::to_milli;
        // nano -> sec
        static time_t to_sec(time_t nano);
        static constexpr auto to_seconds = Time::to_sec;
        // nano -> min
        static time_t to_min(time_t nano);
        static constexpr auto to_minutes = Time::to_min;
        // nano -> hour
        static time_t to_hours(time_t nano);
        // nano -> day
        static time_t to_days(time_t nano);
        // nano -> seconds,milliseconds
        static double to_double(time_t nano);
        static double to_double_milliseconds(time_t nano);
        // nano -> hz
        static double to_freq(time_t nano);
        static constexpr auto to_hz = Time::to_freq;
        static constexpr auto to_frequency = Time::to_freq;

        // chrono -> nano
        template <typename T>
        static constexpr std::chrono::duration<int64_t, T> to_duration(time_t nano)
        {
            // seconds -> nano / 1E9        = nano / (1E9 / 1) / 1
            // milliseconds -> nano / 1E6   = nano / (1E9 / 1E3) / 1
            // nano -> nano / 1             = nano / (1E9 / 1E9) / 1
            // min -> (nano / 1E9) / 60     = nano / (1E9 / 1) / 60
            return std::chrono::duration<int64_t, T>((nano
                / (std::chrono::duration<int64_t, std::nano>::period::den
                    / std::chrono::duration<int64_t, T>::period::den))
                        / std::chrono::duration<int64_t, T>::period::num);
        }

        // micro -> nano
        static time_t micro(time_t micro);
        static constexpr auto us = Time::micro;
        static constexpr auto microseconds = Time::micro;
        // milli -> nano
        static time_t milli(time_t milli);
        static constexpr auto ms = Time::milli;
        static constexpr auto milliseconds = Time::milli;
        // sec -> nano
        static time_t sec(time_t sec);
        static constexpr auto seconds = Time::sec;
        // min -> nano
        static time_t min(time_t min);
        static constexpr auto minutes = Time::min;
        // hour -> nano
        static time_t hours(time_t hour);
        // day -> nano
        static time_t days(time_t day);
        // hz -> nano
        static time_t freq(double hz);
        static constexpr auto frequency = Time::freq;
        static constexpr auto hz = Time::freq;
        // seconds,milliseconds -> nano
        static time_t from_double(double sec_milli);
        static time_t from_double_milliseconds(double milli_micro);

        // chrono -> nano
        template <typename T>
        static constexpr time_t duration(std::chrono::duration<int64_t, T> duration)
        {
            // seconds -> count() * 1E9     == count() * (1E9 / 1) * 1
            // nano -> count() * 1          == count() * (1E9 / 1E9) * 1
            // min -> count() * 1E9 * 60    == count() * (1E9 / 1) * 60
            return (duration.count()
                * (std::chrono::duration<int64_t, std::nano>::period::den
                    / std::chrono::duration<int64_t, T>::period::den))
                        * std::chrono::duration<int64_t, T>::period::num;
        }

        // tm struct -> nano
        static time_t tm(struct tm & tm);
        // timeval with micro resolution -> nano
        static time_t tv(const struct timeval & tv);
        // timeval with nano resolution -> nano
        static time_t nano_tv(const struct timeval & tv);

        static bool is_leap_year(int year);

    private:
        static std::mutex _unsafe_c_mutex;
};

}

#endif