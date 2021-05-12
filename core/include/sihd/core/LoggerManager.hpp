#ifndef __SIHD_CORE_LOGGERMANAGER_HPP__
# define __SIHD_CORE_LOGGERMANAGER_HPP__

# include <sihd/core/ILogger.hpp>
# include <sihd/core/LogInfo.hpp>
# include <list>
# include <mutex>

namespace sihd::core
{

class LoggerManager
{
    public:

        LoggerManager();
        virtual ~LoggerManager();

        bool    has_logger(ILogger *logger);

        bool    add_logger(ILogger *logger);
        bool    remove_logger(ILogger *logger);
        void    delete_loggers();

        void    log(const char *src, LogLevel level, const char *message, ...);

        static LoggerManager    _g_singleton;
        static LoggerManager    *get();
        static bool add(ILogger *logger);
        static bool rm(ILogger *logger);
        static void clear();

    private:
        std::list<ILogger *>::iterator  _find(ILogger *logger);
        std::list<ILogger *>    _loggers;
        std::mutex  _mutex;
};

}

#endif 