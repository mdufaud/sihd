#ifndef __SIHD_UTIL_LOGGERMANAGER_HPP__
# define __SIHD_UTIL_LOGGERMANAGER_HPP__

# include <sihd/util/ALogger.hpp>
# include <sihd/util/ALogFilterer.hpp>
# include <sihd/util/LogInfo.hpp>
# include <sihd/util/BasicLogger.hpp>
# include <list>
# include <mutex>

namespace sihd::util
{

class LoggerManager:    public ALogFilterer
{
    public:

        LoggerManager();
        virtual ~LoggerManager();

        bool    has_logger(ALogger *logger);
        bool    add_logger(ALogger *logger);
        bool    remove_logger(ALogger *logger);
        void    delete_loggers();

        void    log(const char *src, LogLevel level, const char *message);

        static LoggerManager    _g_singleton;
        static LoggerManager    *get();

        static void basic();
        static bool add(ALogger *logger);
        static bool rm(ALogger *logger);
        static bool filter(ILoggerFilter *filter);
        static bool unfilter(ILoggerFilter *filter);
        static void clear_loggers();
        static void clear_filters();

    private:
        std::list<ALogger *>::iterator  _find(ALogger *logger);
        std::list<ALogger *>    _loggers;
        std::mutex  _mutex;
};

}

#endif 