#ifndef __SIHD_SYS_SERVICES_HPP__
#define __SIHD_SYS_SERVICES_HPP__

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sihd::sys
{

std::optional<uint16_t> service_port(std::string_view name, std::string_view proto = "tcp");
std::optional<std::string> service_name(uint16_t port, std::string_view proto = "tcp");

} // namespace sihd::sys

#endif
