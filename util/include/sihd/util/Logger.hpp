#ifndef __SIHD_UTIL_LOGGER_HPP__
#define __SIHD_UTIL_LOGGER_HPP__

#include <string>
#include <string_view>

#include <fmt/format.h>
#include <fmt/printf.h>

#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/macro.hpp>

#if SIHD_LOGGING_OFF

# define SIHD_CERR(message)
# define SIHD_COUTV(message)
# define SIHD_COUT(message)

# define SIHD_LOG_LVL(level, message, ...)
# define SIHD_LOG_LVL_FORMAT(level, message, ...)
# define SIHD_LOG_FORMAT(level, message, ...)
# define SIHD_LOG(logger, level, message)

# define SIHD_NEW_LOGGER(name)
# define SIHD_LOGGER

# define SIHD_TRACE(message)

#else

# define SIHD_COUT(message, ...) fmt::print(message, ##__VA_ARGS__);
# define SIHD_COUTV(message, ...) fmt::print(#message " = {}\n", message);
# define SIHD_CERR(message, ...) fmt::print(stderr, message, ##__VA_ARGS__);

# define SIHD_LOG_LVL(level, message, ...) __sihd_logger__.log(level, fmt::format(message, ##__VA_ARGS__));
// Log with printf like format
# define SIHD_LOG_LVL_FORMAT(level, message, ...) __sihd_logger__.log(level, fmt::sprintf(message, ##__VA_ARGS__));
# define SIHD_LOG(level, message, ...) SIHD_LOG_LVL(sihd::util::LogLevel::level, message, ##__VA_ARGS__)
// Log with printf like format
# define SIHD_LOG_FORMAT(level, message, ...) SIHD_LOG_LVL_FORMAT(sihd::util::LogLevel::level, message, ##__VA_ARGS__)

# define SIHD_LOG_EMERG(message, ...) SIHD_LOG(emergency, message, ##__VA_ARGS__);
# define SIHD_LOG_ALERT(message, ...) SIHD_LOG(alert, message, ##__VA_ARGS__);
# define SIHD_LOG_CRIT(message, ...) SIHD_LOG(critical, message, ##__VA_ARGS__);
# define SIHD_LOG_ERROR(message, ...) SIHD_LOG(error, message, ##__VA_ARGS__);
# define SIHD_LOG_WARN(message, ...) SIHD_LOG(warning, message, ##__VA_ARGS__);
# define SIHD_LOG_NOTICE(message, ...) SIHD_LOG(notice, message, ##__VA_ARGS__);
# define SIHD_LOG_INFO(message, ...) SIHD_LOG(info, message, ##__VA_ARGS__);
# define SIHD_LOG_DEBUG(message, ...) SIHD_LOG(debug, message, ##__VA_ARGS__);

// Declare a new logger into the namespace (use in CPP files)
# define SIHD_NEW_LOGGER(name) sihd::util::Logger __sihd_logger__(name);
// Declare the logger created by SIHD_NEW_LOGGER
# define SIHD_LOGGER extern sihd::util::Logger __sihd_logger__;

# if SIHD_TRACE_OFF
#  define SIHD_TRACE(message)
#  define SIHD_TRACEL(message)
#  define SIHD_TRACE_FORMAT(message)
# else
// Log into debug the file location
#  define SIHD_TRACE(message, ...) SIHD_LOG(debug, "TRACE[" __SIHD_LOC__ "] " message, ##__VA_ARGS__);
// Trace with added function call
#  define SIHD_TRACEL(message, ...)                                                                                    \
      SIHD_LOG(debug, "TRACE[" __SIHD_LOC__ "] {}: " message, __SIHD_FUNCTION__, ##__VA_ARGS__);
// Trace variable value
#  define SIHD_TRACEV(message) SIHD_TRACE(#message " = {}", message);
// Trace with printf like format
#  define SIHD_TRACE_FORMAT(message, ...) SIHD_LOG_FORMAT(debug, "TRACE[" __SIHD_LOC__ "] " message, ##__VA_ARGS__);
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

        std::string name;
};

} // namespace sihd::util

#endif
