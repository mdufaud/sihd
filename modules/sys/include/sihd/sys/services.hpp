#ifndef __SIHD_SYS_SERVICES_HPP__
#define __SIHD_SYS_SERVICES_HPP__

#include <cstdint>
#include <optional>
#include <string>

#include <sihd/util/ZStringView.hpp>

namespace sihd::sys
{

std::optional<uint16_t> service_port(sihd::util::ZStringView name, sihd::util::ZStringView proto = "tcp");
std::optional<std::string> service_name(uint16_t port, sihd::util::ZStringView proto = "tcp");

} // namespace sihd::sys

#endif
