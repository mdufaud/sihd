#ifndef __SIHD_UTIL_LOGINFO_HPP__
# define __SIHD_UTIL_LOGINFO_HPP__

# include <time.h>
# include <string>
# include <sihd/util/Thread.hpp>

namespace sihd::util
{

enum LogLevel
{
    none = 0,
    debug,
    info,
    warning,
    error,
    critical,
};

class LogInfo
{
    public:
        LogInfo(const std::string & source, LogLevel level);
        virtual ~LogInfo();

        std::string     source;
        LogLevel        level;
        std::string     level_str;
        std::thread::id thread_id;
        std::string     thread_id_str;
        std::string     thread_name;
        struct timespec timestamp;

        const std::string & get_level() const;

        static const std::string & get_level(LogLevel level); 
        static LogLevel  string_to_level(const std::string & level);
};

}

#endif 