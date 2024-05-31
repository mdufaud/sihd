#ifndef __SIHD_UTIL_LOGGERMANAGER_HPP__
#define __SIHD_UTIL_LOGGERMANAGER_HPP__

#include <mutex>
#include <vector>

#include <sihd/util/ALogFilterer.hpp>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/LogInfo.hpp>

namespace sihd::util
{

class LoggerManager: public ALogFilterer
{
    public:
        LoggerManager();
        ~LoggerManager();

        bool has_logger(ALogger *logger) const;
        bool add_logger(ALogger *logger);
        bool remove_logger(ALogger *logger);
        void delete_loggers();

        template <typename T>
        bool delete_loggers_type()
        {
            std::lock_guard<std::mutex> l(_mutex);
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

        static LoggerManager *get();

        static void log(const std::string & src, LogLevel level, std::string_view msg);

        static void basic(FILE *output = stderr, bool print_thread_id = false);
        static void console();
        static void thrower(LogLevel filter_level = LogLevel::none, const std::string & filter_source = "");

        static bool add(ALogger *logger);
        static bool rm(ALogger *logger);
        static bool filter(ILoggerFilter *filter);
        static bool rm_filter(ILoggerFilter *filter);
        static void clear_loggers();
        static void clear_filters();

    protected:
        void _filter_and_log(const std::string & src, LogLevel level, std::string_view msg);

    private:
        static LoggerManager _g_singleton;
        std::vector<ALogger *> _loggers_lst;
        mutable std::mutex _mutex;
};

class TmpLoggerAdder
{
    public:
        TmpLoggerAdder(ALogger *logger): _logger_ptr(logger) { LoggerManager::add(logger); }

        ~TmpLoggerAdder() { LoggerManager::rm(_logger_ptr); };

    private:
        ALogger *_logger_ptr;
};

class TmpLoggerFilterAdder
{
    public:
        TmpLoggerFilterAdder(ILoggerFilter *filter): _filter_ptr(filter) { LoggerManager::filter(filter); }

        ~TmpLoggerFilterAdder() { LoggerManager::rm_filter(_filter_ptr); };

    private:
        ILoggerFilter *_filter_ptr;
};

} // namespace sihd::util

#endif