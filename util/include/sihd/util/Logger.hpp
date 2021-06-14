#ifndef __SIHD_UTIL_LOGGER_HPP__
# define __SIHD_UTIL_LOGGER_HPP__

# include <sihd/util/str.hpp>
# include <sihd/util/LoggerManager.hpp>

# ifdef SIHD_LOGGING_OFF
#  define LOG_FORMAT(level, message, ...)
#  define LOG_LEVEL(logger, level, msg)
#  define NEW_LOGGER(name)
#  define DEL_LOGGER
#  define LOGGER
# else
#  define LOG_LEVEL(logger, level, msg) { \
    std::ostringstream __loss; \
    __loss << msg; \
    logger->log(sihd::util::LogLevel::level, __loss.str().c_str()); \
}
#  define LOG(level, msg) LOG_LEVEL(__logger__, level, msg);
#  define LOG_FORMAT(level, message, ...) __logger__->log(level, sihd::util::str::format(message, ##__VA_ARGS__).c_str());

#  define LOG_DEBUG(message, ...) LOG_FORMAT(sihd::util::LogLevel::debug, message, ##__VA_ARGS__);
#  define LOG_INFO(message, ...) LOG_FORMAT(sihd::util::LogLevel::info, message, ##__VA_ARGS__);
#  define LOG_WARN(message, ...) LOG_FORMAT(sihd::util::LogLevel::warning, message, ##__VA_ARGS__);
#  define LOG_ERROR(message, ...) LOG_FORMAT(sihd::util::LogLevel::error, message, ##__VA_ARGS__);
#  define LOG_CRITICAL(message, ...) LOG_FORMAT(sihd::util::LogLevel::critical, message, ##__VA_ARGS__);

#  define LOGGER extern sihd::util::Logger *__logger__;
#  define NEW_LOGGER(name) sihd::util::Logger *__logger__ = new sihd::util::Logger(name);
#  define DEL_LOGGER { \
    delete __logger__; \
    __logger__ = nullptr; \
}
# endif

# ifdef SIHD_TRACE_OFF
#  define TRACE(msg)
# else
#  define TRACE(msg) LOG(debug, "TRACE[" << __FILE__ << ":" << __LINE__ << "] " << msg);
# endif

namespace sihd::util
{

class Logger
{
    public:
        Logger(const std::string & name);
        virtual ~Logger();

        void    debug(const char *msg);
        void    info(const char *msg);
        void    warning(const char *msg);
        void    error(const char *msg);
        void    critical(const char *msg);

        void    log(LogLevel level, const char *msg);

        std::string     name;
};

}

#endif 