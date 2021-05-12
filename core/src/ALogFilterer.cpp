#include <sihd/core/ALogFilterer.hpp>

namespace sihd::core
{

ALogFilterer::ALogFilterer()
{
}

ALogFilterer::~ALogFilterer()
{
    this->delete_filters();
}

bool    ALogFilterer::has_filter(ILoggerFilter *filter)
{
    return std::find(_filters.begin(), _filters.end(), filter) != _filters.end();
}

bool    ALogFilterer::add_filter(ILoggerFilter *filter)
{
    bool has = this->has_filter(filter);
    if (!has)
        _filters.push_back(filter);
    return !has;
}

bool    ALogFilterer::remove_filter(ILoggerFilter *filter)
{
    auto it = this->_find(filter);
    if (it != _filters.end())
    {
        _filters.erase(it);
        delete *it;
    }
    return it != _filters.end();
}

void    ALogFilterer::delete_filters()
{
    for (ILoggerFilter *filter : _filters)
    {
        delete filter;
    }
    _filters.clear();
}

bool    ALogFilterer::should_filter(const LogInfo & info, const char *msg)
{
    for (ILoggerFilter *filter : _filters)
    {
        if (filter->filter(info, msg))
            return true;
    }
    return false;
}

std::list<ILoggerFilter *>::iterator  ALogFilterer::_find(ILoggerFilter *filter)
{
    return std::find(_filters.begin(), _filters.end(), filter);
}

}