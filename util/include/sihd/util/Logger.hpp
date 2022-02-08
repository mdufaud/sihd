#ifndef __SIHD_UTIL_LOGGER_HPP__
# define __SIHD_UTIL_LOGGER_HPP__

# include <iostream>
# include <string>
# include <sihd/util/Str.hpp>
# include <sihd/util/LoggerManager.hpp>

# ifdef SIHD_LOGGING_OFF
#  define SIHD_LOG_FORMAT(level, message, ...)
#  define SIHD_LOG_LEVEL(logger, level, msg)
#  define SIHD_NEW_LOGGER(name)
#  define SIHD_LOGGER
# else
#  define SIHD_LOG_LEVEL(logger, level, msg) { \
    std::ostringstream __loss; \
    __loss << msg; \
    logger.log(sihd::util::LogLevel::level, __loss.str().c_str()); \
}
#  define SIHD_LOG(level, msg) SIHD_LOG_LEVEL(__sihd_logger__, level, msg);
#  define SIHD_LOG_FORMAT(level, message, ...) __sihd_logger__.log(level, sihd::util::Str::format(message, ##__VA_ARGS__).c_str());

#  define SIHD_LOG_DEBUG(message, ...) SIHD_LOG_FORMAT(sihd::util::LogLevel::debug, message, ##__VA_ARGS__);
#  define SIHD_LOG_INFO(message, ...) SIHD_LOG_FORMAT(sihd::util::LogLevel::info, message, ##__VA_ARGS__);
#  define SIHD_LOG_WARN(message, ...) SIHD_LOG_FORMAT(sihd::util::LogLevel::warning, message, ##__VA_ARGS__);
#  define SIHD_LOG_ERROR(message, ...) SIHD_LOG_FORMAT(sihd::util::LogLevel::error, message, ##__VA_ARGS__);
#  define SIHD_LOG_CRITICAL(message, ...) SIHD_LOG_FORMAT(sihd::util::LogLevel::critical, message, ##__VA_ARGS__);

#  define SIHD_LOGGER extern sihd::util::Logger __sihd_logger__;
#  define SIHD_NEW_LOGGER(name) sihd::util::Logger __sihd_logger__(name);
# endif

# if defined(SIHD_TRACE_OFF) || defined(SIHD_LOGGING_OFF)
#  define SIHD_TRACE(msg)
# else
#  define SIHD_TRACE(msg) SIHD_LOG(debug, "TRACE[" << __FILE__ << ":" << __LINE__ << "] " << msg);
# endif

namespace sihd::util
{

class Logger
{
    public:
        Logger(const std::string & name);
        virtual ~Logger();

        void debug(const char *msg);
        void info(const char *msg);
        void warning(const char *msg);
        void error(const char *msg);
        void critical(const char *msg);

        void log(LogLevel level, const char *msg);

        std::string name;
};

}

#endif