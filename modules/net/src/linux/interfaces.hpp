#ifndef __SIHD_NET_LINUX_INTERFACES_HPP__
#define __SIHD_NET_LINUX_INTERFACES_HPP__

#include <cstdint>
#include <optional>
#include <string>

namespace sihd::net::interfaces
{

std::optional<std::string> name_of(uint32_t index);
std::optional<uint32_t> index_of(const std::string & name);

} // namespace sihd::net::interfaces

#endif
