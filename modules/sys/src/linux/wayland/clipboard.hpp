#ifndef __SIHD_SYS_WAYLAND_CLIPBOARD_HPP__
#define __SIHD_SYS_WAYLAND_CLIPBOARD_HPP__

// Wayland clipboard backend of the dispatcher in src/linux/clipboard.cpp.
// One-shot set (serves the first consumer, then drops ownership); bounded get.
// The set deadline is computed once by the dispatcher and passed down.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/clipboard.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/time.hpp>

namespace sihd::sys::clipboard::wayland
{

inline constexpr sihd::util::Duration get_timeout = sihd::util::time::sec(1);

#if defined(SIHD_COMPILE_WITH_WAYLAND)

std::vector<RawContent> get_raw();
std::vector<RawContent> get_raw(const std::vector<std::string_view> & wanted_mimes);
bool set_raw(const std::vector<RawContentView> & contents, sihd::util::Timestamp deadline);

#else

inline std::vector<RawContent> get_raw()
{
    return {};
}

inline std::vector<RawContent> get_raw([[maybe_unused]] const std::vector<std::string_view> & wanted_mimes)
{
    return {};
}

inline bool set_raw([[maybe_unused]] const std::vector<RawContentView> & contents,
                    [[maybe_unused]] sihd::util::Timestamp deadline)
{
    return false;
}

#endif

} // namespace sihd::sys::clipboard::wayland

#endif
