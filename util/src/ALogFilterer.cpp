#include <sihd/util/ALogFilterer.hpp>
#include <sihd/util/container.hpp>

namespace sihd::util
{

ALogFilterer::ALogFilterer() {}

ALogFilterer::~ALogFilterer()
{
    this->delete_filters();
}

bool ALogFilterer::has_filter(ILoggerFilter *filter) const
{
    std::lock_guard<std::mutex> l(_mutex);
    return container::contains(_filters_lst, filter);
}

bool ALogFilterer::add_filter(ILoggerFilter *filter)
{
    std::lock_guard<std::mutex> l(_mutex);
    return container::emplace_back_unique(_filters_lst, filter);
}

bool ALogFilterer::remove_filter(ILoggerFilter *filter)
{
    std::lock_guard<std::mutex> l(_mutex);
    return container::erase(_filters_lst, filter);
}

void ALogFilterer::delete_filters()
{
    std::lock_guard<std::mutex> l(_mutex);
    for (ILoggerFilter *filter : _filters_lst)
    {
        delete filter;
    }
    _filters_lst.clear();
}

bool ALogFilterer::should_filter(const LogInfo & info, std::string_view msg) const
{
    std::lock_guard<std::mutex> l(_mutex);
    for (ILoggerFilter *filter : _filters_lst)
    {
        if (filter->filter(info, msg))
            return true;
    }
    return false;
}

} // namespace sihd::util