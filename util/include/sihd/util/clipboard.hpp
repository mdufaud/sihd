#ifndef __SIHD_UTIL_CLIPBOARD_HPP__
#define __SIHD_UTIL_CLIPBOARD_HPP__

#include <optional>
#include <string_view>

#include <sihd/util/platform.hpp>

namespace sihd::util::clipboard
{

#if defined(SIHD_COMPILE_WITH_X11) || defined(SIHD_COMPILE_WITH_WAYLAND) || defined(__SIHD_WINDOWS__)
constexpr bool usable = true;
#else
constexpr bool usable = false;
#endif

bool set(std::string_view str);
std::optional<std::string> get();

} // namespace sihd::util::clipboard

#endif