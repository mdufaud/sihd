#ifndef __SIHD_UTIL_LOGGERMANAGER_HPP__
# define __SIHD_UTIL_LOGGERMANAGER_HPP__

# include <vector>
# include <mutex>

# include <sihd/util/ALogger.hpp>
# include <sihd/util/ALogFilterer.hpp>
# include <sihd/util/LogInfo.hpp>

namespace sihd::util
{

class LoggerManager: public ALogFilterer
{
    public:
        LoggerManager();
        virtual ~LoggerManager();

        bool has_logger(ALogger *logger) const;
        bool add_logger(ALogger *logger);
        bool remove_logger(ALogger *logger);
        void delete_loggers();

        template <typename T>
        bool delete_logger_type()
        {
            T *logcast;
            bool found = false;
            auto it = _loggers_lst.begin();
            while (it != _loggers_lst.end())
            {
                logcast = dynamic_cast<T *>(*it);
                if (logcast != nullptr)
                {
                    delete logcast;
                    it = _loggers_lst.erase(it);
                    found = true;
                }
            }
            return found;
        }

        void log(const std::string & src, LogLevel level, std::string_view msg);

        static LoggerManager *get();

        static void basic(FILE *output = stderr, bool print_thread_id = false);
        static void console();
        static bool add(ALogger *logger);
        static bool rm(ALogger *logger);
        static bool filter(ILoggerFilter *filter);
        static bool rm_filter(ILoggerFilter *filter);
        static void clear_loggers();
        static void clear_filters();

    private:
        static LoggerManager _g_singleton;

        std::vector<ALogger *>::const_iterator _find(ALogger *logger) const;
        std::vector<ALogger *> _loggers_lst;
        std::mutex _mutex;
};

class TmpLogger
{
    public:
        TmpLogger(ALogger *logger): _logger_ptr(logger)
        {
            LoggerManager::get()->add_logger(logger);
        }

        ~TmpLogger()
        {
            LoggerManager::get()->remove_logger(_logger_ptr);
        };

    private:
        ALogger *_logger_ptr;
};

class TmpLoggerFilter
{
    public:
        TmpLoggerFilter(ILoggerFilter *filter): _filter_ptr(filter)
        {
            LoggerManager::get()->add_filter(filter);
        }

        ~TmpLoggerFilter()
        {
            LoggerManager::get()->remove_filter(_filter_ptr);
        };

    private:
        ILoggerFilter *_filter_ptr;
};

}

#endif