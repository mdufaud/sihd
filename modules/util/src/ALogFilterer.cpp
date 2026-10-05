#include <algorithm>

#include <sihd/util/ALogFilterer.hpp>
#include <sihd/util/container.hpp>

namespace sihd::util
{

ALogFilterer::ALogFilterer() = default;

ALogFilterer::~ALogFilterer()
{
    this->delete_filters();
}

bool ALogFilterer::has_filter(ILoggerFilter *filter) const
{
    std::lock_guard<std::mutex> l(_filters_mutex);
    return container::contains(_filters_lst, filter);
}

bool ALogFilterer::add_filter(ILoggerFilter *filter)
{
    std::lock_guard<std::mutex> l(_filters_mutex);
    const bool added = container::emplace_back_unique(_filters_lst, filter);
    if (added)
        _filters_count.store(_filters_lst.size(), std::memory_order_relaxed);
    return added;
}

bool ALogFilterer::remove_filter(ILoggerFilter *filter)
{
    std::lock_guard<std::mutex> l(_filters_mutex);
    const bool removed = container::erase(_filters_lst, filter);
    if (removed)
        _filters_count.store(_filters_lst.size(), std::memory_order_relaxed);
    return removed;
}

void ALogFilterer::delete_filters()
{
    std::lock_guard<std::mutex> l(_filters_mutex);
    for (ILoggerFilter *filter : _filters_lst)
    {
        delete filter;
    }
    _filters_lst.clear();
    _filters_count.store(0, std::memory_order_relaxed);
}

bool ALogFilterer::should_filter(const LogInfo & info) const
{
    if (_filters_count.load(std::memory_order_relaxed) == 0)
        return false;
    std::lock_guard<std::mutex> l(_filters_mutex);
    for (ILoggerFilter *filter : _filters_lst)
    {
        if (filter->filter(info))
            return true;
    }
    return false;
}

bool ALogFilterer::should_filter(const LogInfo & info, std::string_view msg) const
{
    if (_filters_count.load(std::memory_order_relaxed) == 0)
        return false;
    std::lock_guard<std::mutex> l(_filters_mutex);
    for (ILoggerFilter *filter : _filters_lst)
    {
        if (filter->filter(info) || filter->filter(info, msg))
            return true;
    }
    return false;
}

} // namespace sihd::util
