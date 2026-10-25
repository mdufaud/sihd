#include <utility>

#include <sihd/curl/HeaderList.hpp>
#include <sihd/util/Result.hpp>

#include "impl.hpp"

using namespace sihd::util;
using enum sihd::util::ErrorCode;

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

Result<void> HeaderList::append(ZStringView line)
{
    // curl sends the line as is: a line break would smuggle another header or request
    if (line.view().find_first_of(std::string_view("\r\n\0", 3)) != std::string_view::npos)
        return std::unexpected(Error(invalid_argument, "header line holds a line break or NUL byte"));
    curl_slist *list = curl_slist_append(_impl->list, line.c_str());
    if (list == nullptr)
        return std::unexpected(Error(out_of_memory, "could not append header line"));
    _impl->list = list;
    ++_impl->size;
    return {};
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
