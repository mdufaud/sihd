#include <utility>

#include <sihd/curl/HeaderList.hpp>

#include "impl.hpp"

namespace sihd::curl
{

HeaderList::HeaderList(): _impl(new HeaderListImpl()) {}

HeaderList::~HeaderList()
{
    if (_impl != nullptr)
    {
        if (_impl->list != nullptr)
            curl_slist_free_all(_impl->list);
        delete _impl;
    }
}

HeaderList::HeaderList(HeaderList && other): _impl(other._impl)
{
    other._impl = new HeaderListImpl();
}

HeaderList & HeaderList::operator=(HeaderList && other)
{
    if (this != &other)
        std::swap(_impl, other._impl);
    return *this;
}

bool HeaderList::append(std::string_view line)
{
    // curl_slist_append takes a null-terminated string and copies it
    std::string str(line);
    curl_slist *list = curl_slist_append(_impl->list, str.c_str());
    if (list == nullptr)
        return false;
    _impl->list = list;
    ++_impl->size;
    return true;
}

bool HeaderList::empty() const
{
    return _impl->list == nullptr;
}

size_t HeaderList::size() const
{
    return _impl->size;
}

std::vector<std::string> HeaderList::lines() const
{
    return slist_lines(_impl->list);
}

} // namespace sihd::curl
