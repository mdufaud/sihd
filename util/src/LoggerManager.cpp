#include <algorithm>

#include <sihd/util/BasicLogger.hpp>
#include <sihd/util/ConsoleLogger.hpp>
#include <sihd/util/LoggerManager.hpp>

namespace sihd::util
{

LoggerManager::LoggerManager() {}

LoggerManager::~LoggerManager()
{
    this->delete_loggers();
}

std::vector<ALogger *>::const_iterator LoggerManager::_find(ALogger *logger) const
{
    return std::find(_loggers_lst.cbegin(), _loggers_lst.cend(), logger);
}

bool LoggerManager::has_logger(ALogger *logger) const
{
    return this->_find(logger) != _loggers_lst.end();
}

bool LoggerManager::add_logger(ALogger *logger)
{
    std::lock_guard<std::mutex> l(_mutex);
    bool has = this->has_logger(logger);
    if (!has)
        _loggers_lst.push_back(logger);
    return !has;
}

bool LoggerManager::remove_logger(ALogger *logger)
{
    std::lock_guard<std::mutex> l(_mutex);
    auto it = this->_find(logger);
    if (it != _loggers_lst.end())
    {
        _loggers_lst.erase(it);
    }
    return it != _loggers_lst.end();
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

void LoggerManager::log(const std::string & src, LogLevel level, std::string_view msg)
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

bool LoggerManager::add(ALogger *logger)
{
    return get()->add_logger(logger);
}

bool LoggerManager::rm(ALogger *logger)
{
    return get()->remove_logger(logger);
}

bool LoggerManager::filter(ILoggerFilter *filter)
{
    return get()->add_filter(filter);
}

bool LoggerManager::rm_filter(ILoggerFilter *filter)
{
    return get()->remove_filter(filter);
}

void LoggerManager::clear_loggers()
{
    get()->delete_loggers();
}

void LoggerManager::clear_filters()
{
    get()->delete_filters();
}

void LoggerManager::basic(FILE *output, bool print_thread_id)
{
    get()->add_logger(new BasicLogger(output, print_thread_id));
}

void LoggerManager::console()
{
    get()->add_logger(new ConsoleLogger());
}

} // namespace sihd::util