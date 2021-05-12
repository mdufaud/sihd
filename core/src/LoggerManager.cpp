#include <sihd/core/LoggerManager.hpp>

namespace sihd::core
{

LoggerManager::LoggerManager()
{   
}

LoggerManager::~LoggerManager()
{
    this->delete_loggers();
}

bool    LoggerManager::has_logger(ALogger *logger)
{
    if (std::find(_loggers.begin(), _loggers.end(), logger) != _loggers.end())
        return true;
    return false;
}

std::list<ALogger *>::iterator  LoggerManager::_find(ALogger *logger)
{
    return std::find(_loggers.begin(), _loggers.end(), logger);
}

bool    LoggerManager::add_logger(ALogger *logger)
{
    std::lock_guard<std::mutex> l(_mutex);
    bool has = this->has_logger(logger);
    if (!has)
        _loggers.push_back(logger);
    return !has;
}

bool    LoggerManager::remove_logger(ALogger *logger)
{
    std::lock_guard<std::mutex> l(_mutex);
    auto it = this->_find(logger);
    if (it != _loggers.end())
    {
        _loggers.erase(it);
        delete *it;
    }
    return it != _loggers.end();
}

void    LoggerManager::delete_loggers()
{
    std::lock_guard<std::mutex> l(_mutex);
    for (ALogger *logger : _loggers)
    {
        delete logger;
    }
    _loggers.clear();
}

void    LoggerManager::log(const char *src, LogLevel level, const char *msg)
{
    std::lock_guard<std::mutex> l(_mutex);
    LogInfo info(src, level);
    if (this->should_filter(info, msg))
        return ;
    for (ALogger *logger : _loggers)
    {
        if (logger->should_filter(info, msg) == false)
            logger->log(info, msg);
    }
}

LoggerManager   LoggerManager::_g_singleton;
LoggerManager   *LoggerManager::get()
{
    return &_g_singleton;
}

bool    LoggerManager::add(ALogger *logger)
{
    return get()->add_logger(logger);
}

bool    LoggerManager::rm(ALogger *logger)
{
    return get()->remove_logger(logger);
}

bool    LoggerManager::filter(ILoggerFilter *filter)
{
    return get()->add_filter(filter);
}

bool    LoggerManager::unfilter(ILoggerFilter *filter)
{
    return get()->remove_filter(filter);
}

void    LoggerManager::clear_loggers()
{
    get()->delete_loggers();
}

void    LoggerManager::clear_filters()
{
    get()->delete_filters();
}

}