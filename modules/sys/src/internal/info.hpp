#ifndef __SIHD_SYS_INTERNAL_INFO_HPP__
#define __SIHD_SYS_INTERNAL_INFO_HPP__

#include <string>
#include <string_view>

namespace sihd::sys::internal
{

// placeholders BIOSes leave in DMI fields ("System Product Name", ...)
bool is_dmi_placeholder(std::string_view value);
// system DMI values fall back to base board ones when placeholder or empty
std::string dmi_identity_field(std::string_view system, std::string_view board);

} // namespace sihd::sys::internal

#endif
