#ifndef __SIHD_UTIL_LOGINFO_HPP__
# define __SIHD_UTIL_LOGINFO_HPP__

# include <time.h>
# include <string>
# include <string_view>
# include <sihd/util/Thread.hpp>

namespace sihd::util
{

enum LogLevel
{
    none = 1000,
    emergency = 0, // A panic condition
    alert, // A condition that should be corrected immediately, such as a corrupted system database
    critical, // Hard device errors
    error,
    warning,
    notice, // Conditions that are not error conditions, but that may require special handling
    info, // Confirmation that the program is working as expected
    debug // Messages that contain information normally of use only when debugging a program
};

class LogInfo
{
    public:
        LogInfo(const std::string & source, LogLevel level);
        ~LogInfo();

        std::string_view source;
        LogLevel level;
        const char *strlevel;
        pthread_t thread_id;
        std::string thread_id_str;
        std::string_view thread_name;
        struct timespec timestamp;

        static const char *level_str(LogLevel level);
        static LogLevel level_from_str(std::string_view level);
};

}

#endif