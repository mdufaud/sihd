#include <sihd/util/LoggerManager.hpp>
#include <algorithm>

namespace sihd::util
{

LoggerManager::LoggerManager()
{   
}

LoggerManager::~LoggerManager()
{
    this->remove_loggers();
}

std::list<ALogger *>::iterator  LoggerManager::_find(ALogger *logger)
{
    return std::find(_loggers_lst.begin(), _loggers_lst.end(), logger);
}

bool    LoggerManager::has_logger(ALogger *logger)
{
    return this->_find(logger) != _loggers_lst.end();
}

bool    LoggerManager::add_logger(ALogger *logger)
{
    std::lock_guard<std::mutex> l(_mutex);
    bool has = this->has_logger(logger);
    if (!has)
        _loggers_lst.push_back(logger);
    return !has;
}

bool    LoggerManager::remove_logger(ALogger *logger)
{
    std::lock_guard<std::mutex> l(_mutex);
    auto it = this->_find(logger);
    if (it != _loggers_lst.end())
    {
        _loggers_lst.erase(it);
        delete *it;
    }
    return it != _loggers_lst.end();
}

void    LoggerManager::remove_loggers()
{
    std::lock_guard<std::mutex> l(_mutex);
    for (ALogger *logger : _loggers_lst)
    {
        delete logger;
    }
    _loggers_lst.clear();
}

void    LoggerManager::log(const char *src, LogLevel level, const char *msg)
{
    std::lock_guard<std::mutex> l(_mutex);
    LogInfo info(src, level);
    // global filter
    if (this->should_filter(info, msg))
        return ;
    for (ALogger *logger: _loggers_lst)
    {
        // filter by logger
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

bool    LoggerManager::remove(ALogger *logger)
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
    get()->remove_loggers();
}

void    LoggerManager::clear_filters()
{
    get()->remove_filters();
}

void    LoggerManager::basic(FILE *output, bool print_thread_id)
{
    get()->add_logger(new BasicLogger(output, print_thread_id));
}

}