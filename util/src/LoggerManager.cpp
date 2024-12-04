#include <sihd/util/LoggerConsole.hpp>
#include <sihd/util/LoggerFilterLevel.hpp>
#include <sihd/util/LoggerFilterSource.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/LoggerThrow.hpp>
#include <sihd/util/container.hpp>

namespace sihd::util
{

LoggerManager::LoggerManager() {}

LoggerManager::~LoggerManager()
{
    this->delete_loggers();
}

bool LoggerManager::has_logger(ALogger *logger) const
{
    std::lock_guard<std::mutex> l(_mutex);
    return container::contains(_loggers_lst, logger);
}

bool LoggerManager::add_logger(ALogger *logger)
{
    std::lock_guard<std::mutex> l(_mutex);
    return container::emplace_unique(_loggers_lst, logger);
}

bool LoggerManager::remove_logger(ALogger *logger)
{
    std::lock_guard<std::mutex> l(_mutex);
    return container::erase(_loggers_lst, logger);
}

void LoggerManager::delete_loggers()
{
    std::lock_guard<std::mutex> l(_mutex);
    for (ALogger *logger : _loggers_lst)
    {
        delete logger;
    }
    _loggers_lst.clear();
}

void LoggerManager::_filter_and_log(const std::string & src, LogLevel level, std::string_view msg)
{
    std::lock_guard<std::mutex> l(_mutex);
    LogInfo info(src, level);
    // global filter
    if (this->should_filter(info, msg))
        return;
    for (ALogger *logger : _loggers_lst)
    {
        // filter by logger
        if (logger->should_filter(info, msg) == false)
            logger->log(info, msg);
    }
}

LoggerManager LoggerManager::_g_singleton;

LoggerManager *LoggerManager::get()
{
    return &_g_singleton;
}

void LoggerManager::log(const std::string & src, LogLevel level, std::string_view msg)
{
    return _g_singleton._filter_and_log(src, level, msg);
}

bool LoggerManager::add(ALogger *logger)
{
    return _g_singleton.add_logger(logger);
}

bool LoggerManager::rm(ALogger *logger)
{
    return _g_singleton.remove_logger(logger);
}

bool LoggerManager::filter(ILoggerFilter *filter)
{
    return _g_singleton.add_filter(filter);
}

bool LoggerManager::rm_filter(ILoggerFilter *filter)
{
    return _g_singleton.remove_filter(filter);
}

void LoggerManager::clear_loggers()
{
    _g_singleton.delete_loggers();
}

void LoggerManager::clear_filters()
{
    _g_singleton.delete_filters();
}

void LoggerManager::stream(FILE *output, bool print_thread_id)
{
    LoggerManager::add(new LoggerStream(output, print_thread_id));
}

void LoggerManager::console()
{
    LoggerManager::add(new LoggerConsole());
}

void LoggerManager::thrower(LogLevel filter_level, const std::string & filter_source)
{
    LoggerThrow *logger = new LoggerThrow();
    if (filter_level != LogLevel::none)
    {
        constexpr bool match = false;
        logger->add_filter(new LoggerFilterLevel(filter_level, match));
    }
    if (filter_source.empty() == false)
    {
        constexpr bool match_only = false;
        logger->add_filter(new LoggerFilterSource(filter_source, match_only));
    }
    LoggerManager::add(logger);
}

} // namespace sihd::util