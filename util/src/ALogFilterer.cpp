#include <sihd/util/ALogFilterer.hpp>
#include <algorithm>

namespace sihd::util
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
    return std::find(_filters_lst.begin(), _filters_lst.end(), filter) != _filters_lst.end();
}

bool    ALogFilterer::add_filter(ILoggerFilter *filter)
{
    bool has = this->has_filter(filter);
    if (!has)
        _filters_lst.push_back(filter);
    return !has;
}

bool    ALogFilterer::remove_filter(ILoggerFilter *filter)
{
    auto it = this->_find(filter);
    if (it != _filters_lst.end())
    {
        _filters_lst.erase(it);
    }
    return it != _filters_lst.end();
}

void    ALogFilterer::delete_filters()
{
    for (ILoggerFilter *filter : _filters_lst)
    {
        delete filter;
    }
    _filters_lst.clear();
}

bool    ALogFilterer::should_filter(const LogInfo & info, std::string_view msg)
{
    for (ILoggerFilter *filter : _filters_lst)
    {
        if (filter->filter(info, msg))
            return true;
    }
    return false;
}

std::list<ILoggerFilter *>::iterator  ALogFilterer::_find(ILoggerFilter *filter)
{
    return std::find(_filters_lst.begin(), _filters_lst.end(), filter);
}

}