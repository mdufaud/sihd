#ifndef __SIHD_UTIL_LOGGERFILTER_HPP__
#define __SIHD_UTIL_LOGGERFILTER_HPP__

#include <sihd/util/ILoggerFilter.hpp>

namespace sihd::util
{

class LoggerFilter: public ILoggerFilter
{
    public:
        struct Options
        {
                std::string message_regex = "";
                std::string source_regex = "";
                std::string thread_regex = "";
                pthread_t thread_eq = 0;
                pthread_t thread_ne = 0;
                LogLevel level_eq = LogLevel::none;
                LogLevel level_gt = LogLevel::none;
                LogLevel level_lt = LogLevel::none;
        };

        LoggerFilter(const Options & options);
        virtual ~LoggerFilter();

    protected:
        bool filter(const LogInfo & info, std::string_view msg);

    private:
        Options _options;
};

} // namespace sihd::util

#endif
