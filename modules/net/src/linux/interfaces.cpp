#include "interfaces.hpp"

#include <array>

#include <net/if.h>

namespace sihd::net::interfaces
{

using namespace sihd::util;

std::optional<std::string> name_of(uint32_t index)
{
    std::array<char, IF_NAMESIZE> name {};
    if (if_indextoname(index, name.data()) == nullptr)
        return std::nullopt;
    return name.data();
}

std::optional<uint32_t> index_of(ZStringView name)
{
    const uint32_t index = if_nametoindex(name.c_str());
    if (index == 0)
        return std::nullopt;
    return index;
}

} // namespace sihd::net::interfaces
