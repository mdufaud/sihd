#ifndef __SIHD_SYS_SERVICES_HPP__
#define __SIHD_SYS_SERVICES_HPP__

#include <cstdint>
#include <optional>
#include <string>

namespace sihd::sys
{

std::optional<uint16_t> service_port(const std::string & name, const std::string & proto = "tcp");
std::optional<std::string> service_name(uint16_t port, const std::string & proto = "tcp");

} // namespace sihd::sys

#endif
