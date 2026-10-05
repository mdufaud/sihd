#include <cstdio>

#include <sihd/util/LoggerAsync.hpp>
#include <sihd/util/LoggerConsole.hpp>
#include <sihd/util/LoggerManager.hpp>
#include <sihd/util/LoggerStream.hpp>
#include <sihd/util/LoggerThrow.hpp>
#include <sihd/util/container.hpp>

namespace sihd::util
{

LoggerManager LoggerManager::_g_singleton;

namespace
{

void install(ALogger *logger, std::optional<LoggerFilter::Options> options)
{
    if (options.has_value())
        logger->add_filter(new LoggerFilter(*options));
    LoggerManager::add(logger);
}

} // namespace

LoggerManager::LoggerManager() = default;

LoggerManager::~LoggerManager()
{
    this->delete_loggers();
}

bool LoggerManager::_enter() const
{
    if (_mutex.try_lock())
    {
        _owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
        return true;
    }
    if (_owner.load(std::memory_order_relaxed) == std::this_thread::get_id())
    {
        if (_warned_reentrant.exchange(true, std::memory_order_relaxed) == false)
            fprintf(stderr, "reentrant log dropped: a logger or filter logs back into LoggerManager\n");
        return false;
    }
    _mutex.lock();
    _owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
    return true;
}

void LoggerManager::_leave() const
{
    _owner.store(std::thread::id {}, std::memory_order_relaxed);
    _mutex.unlock();
}

LoggerManager::Lock::Lock(const LoggerManager & manager): _manager(manager), _locked(manager._enter()) {}

LoggerManager::Lock::~Lock()
{
    if (_locked)
        _manager._leave();
}

LoggerManager::Lock::operator bool() const
{
    return _locked;
}

bool LoggerManager::has_logger(ALogger *logger) const
{
    const Lock lock(*this);
    if (!lock)
        return false;
    return container::contains(_loggers_lst, logger);
}

bool LoggerManager::add_logger(ALogger *logger)
{
    const Lock lock(*this);
    if (!lock)
        return false;
    if (container::emplace_back_unique(_loggers_lst, logger) == false)
        return false;
    _warned_no_sink = false;
    return true;
}

bool LoggerManager::remove_logger(ALogger *logger)
{
    const Lock lock(*this);
    if (!lock)
        return false;
    return container::erase(_loggers_lst, logger);
}

void LoggerManager::delete_loggers()
{
    std::vector<ALogger *> loggers;
    {
        const Lock lock(*this);
        if (!lock)
            return;
        loggers.swap(_loggers_lst);
    }
    // sinks may remove themselves on destruction: delete outside the lock
    for (ALogger *logger : loggers)
    {
        delete logger;
    }
}

bool LoggerManager::_filter_no_format(const LogInfo & info)
{
    const Lock lock(*this);
    if (!lock)
        return false;
    if (_loggers_lst.empty())
    {
        this->_warn_no_sink(info);
        return false;
    }
    if (this->should_filter(info))
        return false;
    for (const ALogger *logger : _loggers_lst)
    {
        if (logger->should_filter(info) == false)
            return true;
    }
    return false;
}

void LoggerManager::_filter_and_log(const LogInfo & info, std::string_view msg)
{
    const Lock lock(*this);
    if (!lock)
        return;
    if (_loggers_lst.empty())
    {
        this->_warn_no_sink(info);
        return;
    }
    if (this->should_filter(info, msg))
        return;
    for (ALogger *logger : _loggers_lst)
    {
        if (logger->should_filter(info, msg) == false)
            logger->log(info, msg);
    }
}

void LoggerManager::_warn_no_sink(const LogInfo & info)
{
    if (_warned_no_sink == false)
    {
        _warned_no_sink = true;
        fprintf(stderr,
                "no logger installed: dropped %s from %.*s\n",
                LogInfo::level_str(info.level),
                (int)info.source.size(),
                info.source.data());
    }
}

LoggerManager *LoggerManager::get()
{
    return &_g_singleton;
}

void LoggerManager::log(const std::string & src, LogLevel level, std::string_view msg)
{
    LogInfo info(src, level);
    _g_singleton._filter_and_log(info, msg);
}

void LoggerManager::log(const LogInfo & info, std::string_view msg)
{
    _g_singleton._filter_and_log(info, msg);
}

bool LoggerManager::should_log(const std::string & src, LogLevel level)
{
    LogInfo info(src, level);
    return _g_singleton._filter_no_format(info);
}

bool LoggerManager::should_log(const LogInfo & info)
{
    return _g_singleton._filter_no_format(info);
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

void LoggerManager::stream(FILE *output, bool print_thread_id, std::optional<LoggerFilter::Options> options)
{
    install(new LoggerStream(output, print_thread_id), options);
}

void LoggerManager::console(std::optional<LoggerFilter::Options> options)
{
    install(new LoggerConsole(), options);
}

void LoggerManager::thrower(std::optional<LoggerFilter::Options> options)
{
    install(new LoggerThrow(), options);
}

void LoggerManager::async(ALogger *target,
                          size_t max_queue_size,
                          std::optional<LoggerFilter::Options> options,
                          std::string source)
{
    install(new LoggerAsync(target, max_queue_size, std::move(source)), options);
}

} // namespace sihd::util
