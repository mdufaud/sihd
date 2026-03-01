#ifndef __SIHD_SYS_CLIPBOARD_HPP__
#define __SIHD_SYS_CLIPBOARD_HPP__

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <sihd/sys/Bitmap.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::sys::clipboard
{

#if defined(SIHD_COMPILE_WITH_X11) || defined(SIHD_COMPILE_WITH_WAYLAND) || defined(__SIHD_WINDOWS__)
constexpr bool usable = true;
#else
constexpr bool usable = false;
#endif

using Content = std::variant<std::string, Bitmap>;

// Set text to clipboard
bool set_text(std::string_view str);
// Set image to clipboard
bool set_image(const Bitmap & bitmap);

// Get text from clipboard (alias)
std::optional<std::string> get_text();
// Get image from clipboard
std::optional<Bitmap> get_image();
// Get either text or image from clipboard (prefers image if both available)
std::optional<Content> get_any();

} // namespace sihd::sys::clipboard

#endif