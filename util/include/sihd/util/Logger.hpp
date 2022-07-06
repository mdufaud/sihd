#ifndef __SIHD_UTIL_LOGGER_HPP__
# define __SIHD_UTIL_LOGGER_HPP__

# include <iostream>
# include <string>
# include <string_view>

# include <fmt/format.h>

# include <sihd/util/macro.hpp>
# include <sihd/util/LoggerManager.hpp>

# ifdef SIHD_LOGGING_OFF
#  define SIHD_LOG_FORMAT(level, message, ...)
#  define SIHD_LOG_LEVEL(logger, level, msg)
#  define SIHD_LOGF(level, message, ...)
#  define SIHD_NEW_LOGGER(name)
#  define SIHD_LOGGER
#  define SIHD_COUT(msg)
#  define SIHD_CERR(msg)
# else
#  define SIHD_COUT(msg) std::cout << msg << std::endl;
#  define SIHD_COUTF(msg, ...) std::cout << fmt::format(msg, ##__VA_ARGS__) << std::endl;
#  define SIHD_CERR(msg) std::cerr << msg << std::endl;
#  define SIHD_CERRF(msg, ...) std::cerr << fmt::format(msg, ##__VA_ARGS__) << std::endl;
#  define SIHD_LOG_LEVEL(logger, level, msg) { \
    std::ostringstream __loss; \
    __loss << msg; \
    logger.log(sihd::util::LogLevel::level, __loss.str()); \
}
#  define SIHD_LOG(level, msg) SIHD_LOG_LEVEL(__sihd_logger__, level, msg);
#  define SIHD_LOGF(level, message, ...) __sihd_logger__.log(sihd::util::LogLevel::level, fmt::format(message, ##__VA_ARGS__));
#  define SIHD_LOG_FORMAT(level, message, ...) __sihd_logger__.log(sihd::util::LogLevel::level, sihd::util::Str::format(message, ##__VA_ARGS__));

#  define SIHD_LOG_EMERG(message, ...) SIHD_LOGF(emergency, message, ##__VA_ARGS__);
#  define SIHD_LOG_ALERT(message, ...) SIHD_LOGF(alert, message, ##__VA_ARGS__);
#  define SIHD_LOG_CRIT(message, ...) SIHD_LOGF(critical, message, ##__VA_ARGS__);
#  define SIHD_LOG_ERROR(message, ...) SIHD_LOGF(error, message, ##__VA_ARGS__);
#  define SIHD_LOG_WARN(message, ...) SIHD_LOGF(warning, message, ##__VA_ARGS__);
#  define SIHD_LOG_NOTICE(message, ...) SIHD_LOGF(notice, message, ##__VA_ARGS__);
#  define SIHD_LOG_INFO(message, ...) SIHD_LOGF(info, message, ##__VA_ARGS__);
#  define SIHD_LOG_DEBUG(message, ...) SIHD_LOGF(debug, message, ##__VA_ARGS__);

#  define SIHD_LOGGER extern sihd::util::Logger __sihd_logger__;
#  define SIHD_NEW_LOGGER(name) sihd::util::Logger __sihd_logger__(name);
# endif

# if defined(SIHD_TRACE_OFF) || defined(SIHD_LOGGING_OFF)
#  define SIHD_TRACE(msg)
#  define SIHD_TRACEF(msg)
# else
#  define SIHD_TRACE(msg) SIHD_LOG(debug, "TRACE[" __FILE__ ":"  __SIHD_STRINGIFY__(__LINE__) "] " << msg);
#  define SIHD_TRACEF(msg, ...) SIHD_LOGF(debug, "TRACE[" __FILE__ ":"  __SIHD_STRINGIFY__(__LINE__) "] " msg, ##__VA_ARGS__);
# endif

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

}

#endif