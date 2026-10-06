#include <optional>
#include <utility>

#include <fmt/core.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerAsync.hpp>
#include <sihd/util/thread.hpp>

namespace sihd::util
{

SIHD_LOGGER;

LoggerAsync::Item::Item(const LogInfo & info, std::string_view msg):
    info(info),
    source(info.source),
    thread_name(info.thread_name),
    msg(msg)
{
    this->_reseat();
}

LoggerAsync::Item::Item(Item && other):
    info(other.info),
    source(std::move(other.source)),
    thread_name(std::move(other.thread_name)),
    msg(std::move(other.msg))
{
    this->_reseat();
}

void LoggerAsync::Item::_reseat()
{
    this->info.source = this->source;
    this->info.thread_name = this->thread_name;
}

LoggerAsync::LoggerAsync(ALogger *target, size_t max_queue_size, std::string source):
    _source(std::move(source)),
    _target(target),
    _max_queue_size(max_queue_size)
{
    _thread = std::jthread([this] { this->_drain(); });
}

LoggerAsync::~LoggerAsync()
{
    _queue.terminate();
    if (_thread.joinable())
        _thread.join();
}

void LoggerAsync::log(const LogInfo & info, std::string_view msg)
{
    if (_queue.push_lazy([&] { return Item(info, msg); }, _max_queue_size) == false)
        _dropped.fetch_add(1, std::memory_order_relaxed);
}

size_t LoggerAsync::dropped() const
{
    return _dropped.load(std::memory_order_relaxed);
}

void LoggerAsync::_drain()
{
    SIHD_UNEXPECTED_LOG(thread::set_name("logger-async"));
    while (const std::optional<Item> item = _queue.pop_wait())
    {
        this->_process(item->info, item->msg);
        // one wakeup per burst, not per message
        while (const std::optional<Item> next = _queue.try_pop())
            this->_process(next->info, next->msg);
        this->_report_drops();
    }
    this->_report_drops();
}

void LoggerAsync::_report_drops()
{
    const size_t dropped = _dropped.exchange(0, std::memory_order_relaxed);
    if (dropped > 0)
        this->_report_dropped(dropped);
}

void LoggerAsync::_process(const LogInfo & info, std::string_view msg)
{
    // the target filters are checked in the drain's thread
    try
    {
        if (_target->should_filter(info, msg) == false)
            _target->log(info, msg);
    }
    catch (...)
    {
    }
}

void LoggerAsync::_report_dropped(size_t count)
{
    // bypasses the target filters: a filtered report loses the drops it accounts for
    try
    {
        _target->log(LogInfo(_source, LogLevel::warning), fmt::format("{} messages dropped", count));
    }
    catch (...)
    {
    }
}

} // namespace sihd::util
