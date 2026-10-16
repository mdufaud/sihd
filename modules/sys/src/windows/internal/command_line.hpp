#ifndef __SIHD_SYS_WINDOWS_INTERNAL_COMMAND_LINE_HPP__
#define __SIHD_SYS_WINDOWS_INTERNAL_COMMAND_LINE_HPP__

#include <span>
#include <string>
#include <string_view>

namespace sihd::sys::internal
{

std::string command_line(std::span<const std::string_view> args);

}

#endif
