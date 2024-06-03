#ifndef __SIHD_UTIL_CLIPBOARD_HPP__
#define __SIHD_UTIL_CLIPBOARD_HPP__

#include <optional>
#include <string_view>

namespace sihd::util::clipboard
{

bool set(std::string_view str);
std::optional<std::string> get();

} // namespace sihd::util::clipboard

#endif