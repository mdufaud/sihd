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

bool    LoggerManager::has_logger(ILogger *logger)
{
    if (std::find(_loggers.begin(), _loggers.end(), logger) != _loggers.end())
        return true;
    return false;
}

std::list<ILogger *>::iterator  LoggerManager::_find(ILogger *logger)
{
    return std::find(_loggers.begin(), _loggers.end(), logger);
}

bool    LoggerManager::add_logger(ILogger *logger)
{
    std::lock_guard<std::mutex> l(_mutex);
    bool has = this->has_logger(logger);
    if (!has)
        _loggers.push_back(logger);
    return !has;
}

bool    LoggerManager::remove_logger(ILogger *logger)
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
    for (ILogger *logger : _loggers)
    {
        delete logger;
    }
    _loggers.clear();
}

void    LoggerManager::log(const char *src, LogLevel level, const char *msg, ...)
{
    std::lock_guard<std::mutex> l(_mutex);
    LogInfo info(src, level);
    for (ILogger *logger : _loggers)
    {
        logger->log(info, msg);
    }
}

LoggerManager   LoggerManager::_g_singleton;
LoggerManager   *LoggerManager::get()
{
    return &_g_singleton;
}

bool    LoggerManager::add(ILogger *logger)
{
    return get()->add_logger(logger);
}

bool    LoggerManager::rm(ILogger *logger)
{
    return get()->remove_logger(logger);
}

void    LoggerManager::clear()
{
    get()->delete_loggers();
}

}