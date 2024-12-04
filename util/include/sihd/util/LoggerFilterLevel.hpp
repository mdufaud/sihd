#ifndef __SIHD_UTIL_LOGGERFILTERLEVEL_HPP__
#define __SIHD_UTIL_LOGGERFILTERLEVEL_HPP__

#include <sihd/util/ILoggerFilter.hpp>

namespace sihd::util
{

class LoggerFilterLevel: public ILoggerFilter
{
    public:
        LoggerFilterLevel(LogLevel min, bool match = false);
        LoggerFilterLevel(const std::string & level, bool match = false);
        ~LoggerFilterLevel();

        bool filter(const LogInfo & info, std::string_view msg);

        LogLevel level;
        bool match;
};

} // namespace sihd::util

#endif