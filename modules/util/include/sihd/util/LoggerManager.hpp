#ifndef __SIHD_UTIL_LOGGERMANAGER_HPP__
#define __SIHD_UTIL_LOGGERMANAGER_HPP__

#include <atomic>
#include <cstdio>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <sihd/util/ALogFilterer.hpp>
#include <sihd/util/ALogger.hpp>
#include <sihd/util/LogInfo.hpp>
#include <sihd/util/LoggerFilter.hpp>

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
            std::vector<ALogger *> loggers;
            {
                const Lock lock(*this);
                if (!lock)
                    return false;
                auto it = _loggers_lst.begin();
                while (it != _loggers_lst.end())
                {
                    T *logcast = dynamic_cast<T *>(*it);
                    if (logcast != nullptr)
                    {
                        loggers.push_back(*it);
                        it = _loggers_lst.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            // sinks may remove themselves on destruction: delete outside the lock
            for (ALogger *logger : loggers)
            {
                delete logger;
            }
            return loggers.empty() == false;
        }

        static LoggerManager *get();

        static void log(const std::string & src, LogLevel level, std::string_view msg);
        static void log(const LogInfo & info, std::string_view msg);

        static void set_level(LogLevel level);
        static bool should_log_level(LogLevel level);
        static bool should_log(const std::string & src, LogLevel level);
        static bool should_log(const LogInfo & info);

        static void stream(FILE *output = stderr,
                           std::string pattern = "",
                           std::optional<LoggerFilter::Options> options = std::nullopt);
        static void console(std::optional<LoggerFilter::Options> options = std::nullopt);
        static void thrower(std::optional<LoggerFilter::Options> options = std::nullopt);
        static void async(ALogger *target,
                          size_t max_queue_size = 8192,
                          std::optional<LoggerFilter::Options> options = std::nullopt,
                          std::string source = "sihd::util::logger_async");

        static bool add(ALogger *logger);
        static bool rm(ALogger *logger);
        static bool filter(ILoggerFilter *filter);
        static bool rm_filter(ILoggerFilter *filter);
        static void clear_loggers();
        static void clear_filters();

    protected:
        void _filter_and_log(const LogInfo & info, std::string_view msg);

        bool _filter_no_format(const LogInfo & info);

        void _warn_no_sink(const LogInfo & info);

    private:
        // the only entry to _mutex: a log-back from the same thread is dropped, not deadlocked
        class Lock
        {
            public:
                Lock(const LoggerManager & manager);
                ~Lock();

                explicit operator bool() const;

            private:
                const LoggerManager & _manager;
                bool _locked;
        };

        static LoggerManager _g_singleton;
        std::vector<ALogger *> _loggers_lst;
        std::atomic<LogLevel> _level {LogLevel::debug};
        bool _warned_no_sink = false;
        mutable std::atomic<std::thread::id> _owner {};
        mutable std::atomic<bool> _warned_reentrant {false};
        mutable std::mutex _mutex;

        bool _enter() const;
        void _leave() const;
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
