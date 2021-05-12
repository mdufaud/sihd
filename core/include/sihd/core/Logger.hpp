#ifndef __SIHD_CORE_LOGGER_HPP__
# define __SIHD_CORE_LOGGER_HPP__

# include <sihd/core/str.hpp>
# include <sihd/core/ILogger.hpp>
# include <sihd/core/LoggerManager.hpp>

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
    logger->log(sihd::core::LogLevel::level, __loss.str().c_str()); \
}
#  define LOG(level, msg) LOG_LEVEL(__logger__, level, msg);
#  define LOG_FORMAT(level, message, ...) __logger__->log(level, sihd::core::str::format(message, ##__VA_ARGS__).c_str());
#  define LOG_INFO(message, ...) LOG_FORMAT(sihd::core::LogLevel::info, message, ##__VA_ARGS__);
#  define LOGGER extern sihd::core::Logger *__logger__;
#  define NEW_LOGGER(name) sihd::core::Logger *__logger__ = new sihd::core::Logger(name);
# endif

namespace sihd::core
{

class Logger
{
    public:
        Logger(const std::string & name);
        virtual ~Logger();

        void    log(LogLevel level, const char *msg);

        std::string     name;
};

}

#endif 