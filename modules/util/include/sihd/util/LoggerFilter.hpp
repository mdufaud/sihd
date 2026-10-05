#ifndef __SIHD_UTIL_LOGGERFILTER_HPP__
#define __SIHD_UTIL_LOGGERFILTER_HPP__

#include <optional>
#include <regex>

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
                LogLevel level_higher = LogLevel::none;
                LogLevel level_lower = LogLevel::none;
        };

        LoggerFilter(const Options & options);
        virtual ~LoggerFilter();

        const Options & options() const;

    protected:
        bool filter(const LogInfo & info) override;
        bool filter(const LogInfo & info, std::string_view msg) override;

    private:
        Options _options;
        std::optional<std::regex> _message_regex;
        std::optional<std::regex> _source_regex;
        std::optional<std::regex> _thread_regex;
};

} // namespace sihd::util

#endif
