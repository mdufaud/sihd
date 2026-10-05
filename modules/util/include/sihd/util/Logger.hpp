#ifndef __SIHD_UTIL_LOGGER_HPP__
#define __SIHD_UTIL_LOGGER_HPP__

#include <expected>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/core.h>
#include <fmt/printf.h>

#include <sihd/util/Error.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/macro.hpp>

#if SIHD_LOGGING_OFF

# define SIHD_CERR(message, ...)
# define SIHD_COUTV(message, ...)
# define SIHD_COUT(message, ...)

# define SIHD_LOG_LVL(level, message, ...)
# define SIHD_LOG_LVL_FORMAT(level, message, ...)
# define SIHD_LOG_FORMAT(level, message, ...)
# define SIHD_LOG(level, message, ...)

// returns true when the expected holds an error
# define SIHD_UNEXPECTED_LOG(expected_value) ((expected_value).has_value() == false)

# define SIHD_NEW_LOGGER(name)
# define SIHD_LOGGER

# define SIHD_TRACE(message, ...)
# define SIHD_TRACEL(message, ...)
# define SIHD_TRACEV(message)
# define SIHD_TRACE_FORMAT(message, ...)

#else

# define SIHD_COUT(message, ...) fmt::print(message, ##__VA_ARGS__)
# define SIHD_COUTV(message, ...) fmt::print(#message " = {}\n", message)
# define SIHD_CERR(message, ...) fmt::print(stderr, message, ##__VA_ARGS__)

# define SIHD_LOG_LVL(level, message, ...) __sihd_logger__.log(level, message, ##__VA_ARGS__)
// Log with printf like format
# define SIHD_LOG_LVL_FORMAT(level, message, ...) __sihd_logger__.log(level, fmt::sprintf(message, ##__VA_ARGS__))
# define SIHD_LOG(level, message, ...) SIHD_LOG_LVL(sihd::util::LogLevel::level, message, ##__VA_ARGS__)
// Log with printf like format
# define SIHD_LOG_FORMAT(level, message, ...) SIHD_LOG_LVL_FORMAT(sihd::util::LogLevel::level, message, ##__VA_ARGS__)

// logs the error of a std::expected<..., sihd::util::Error> with the call site, returns true when it was an error
// SIHD_UNEXPECTED_LOG_NOLOC drops the call site from logs
# if SIHD_UNEXPECTED_LOG_NOLOC
#  define SIHD_UNEXPECTED_LOG(expected_value)                                                                          \
      ::sihd::util::log_unexpected(__sihd_logger__, expected_value, std::source_location {})
# else
#  define SIHD_UNEXPECTED_LOG(expected_value) ::sihd::util::log_unexpected(__sihd_logger__, expected_value)
# endif

// Declare a new logger into the namespace (use in CPP files)
# define SIHD_NEW_LOGGER(name) inline sihd::util::Logger __sihd_logger__(name);
// Declare the logger created by SIHD_NEW_LOGGER
# define SIHD_LOGGER extern sihd::util::Logger __sihd_logger__;

# if SIHD_TRACE_OFF
#  define SIHD_TRACE(message, ...)
#  define SIHD_TRACEL(message, ...)
#  define SIHD_TRACEV(message)
#  define SIHD_TRACE_FORMAT(message, ...)
# else
// Log into debug the file location
#  define SIHD_TRACE(message, ...) SIHD_LOG(debug, "TRACE[" __SIHD_LOC__ "] " message, ##__VA_ARGS__)
// Trace with added function call
#  define SIHD_TRACEL(message, ...)                                                                                    \
      SIHD_LOG(debug, "TRACE[" __SIHD_LOC__ "] {}: " message, __SIHD_FUNCTION__, ##__VA_ARGS__)
// Trace variable value
#  define SIHD_TRACEV(message) SIHD_TRACE(#message " = {}", message)
// Trace with printf like format
#  define SIHD_TRACE_FORMAT(message, ...) SIHD_LOG_FORMAT(debug, "TRACE[" __SIHD_LOC__ "] " message, ##__VA_ARGS__)
# endif

#endif

namespace sihd::util
{

class Logger
{
    public:
        Logger(const std::string & name);
        virtual ~Logger();

        void emergency(std::string_view msg);
        void alert(std::string_view msg);
        void critical(std::string_view msg);
        void error(std::string_view msg);
        void warning(std::string_view msg);
        void notice(std::string_view msg);
        void info(std::string_view msg);
        void debug(std::string_view msg);

        void log(LogLevel level, std::string_view msg);

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        void log(LogLevel level, fmt::format_string<Args...> format, Args &&...args)
        {
            LogInfo info(name, level);
            if (LoggerManager::should_log(info))
                LoggerManager::log(info, fmt::format(format, std::forward<Args>(args)...));
        }

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        void emergency(fmt::format_string<Args...> format, Args &&...args)
        {
            this->log(LogLevel::emergency, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        void alert(fmt::format_string<Args...> format, Args &&...args)
        {
            this->log(LogLevel::alert, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        void critical(fmt::format_string<Args...> format, Args &&...args)
        {
            this->log(LogLevel::critical, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        void error(fmt::format_string<Args...> format, Args &&...args)
        {
            this->log(LogLevel::error, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        void warning(fmt::format_string<Args...> format, Args &&...args)
        {
            this->log(LogLevel::warning, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        void notice(fmt::format_string<Args...> format, Args &&...args)
        {
            this->log(LogLevel::notice, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        void info(fmt::format_string<Args...> format, Args &&...args)
        {
            this->log(LogLevel::info, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
            requires(sizeof...(Args) != 0)
        void debug(fmt::format_string<Args...> format, Args &&...args)
        {
            this->log(LogLevel::debug, format, std::forward<Args>(args)...);
        }

        std::string name;
};

void log_unexpected_error(Logger & logger, const Error & err, const std::source_location & loc);

template <typename T>
bool log_unexpected(Logger & logger,
                    const std::expected<T, Error> & res,
                    const std::source_location & loc = std::source_location::current())
{
    if (res.has_value())
        return false;
    log_unexpected_error(logger, res.error(), loc);
    return true;
}

} // namespace sihd::util

#endif
