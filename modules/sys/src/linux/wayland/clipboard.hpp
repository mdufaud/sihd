#ifndef __SIHD_SYS_WAYLAND_CLIPBOARD_HPP__
#define __SIHD_SYS_WAYLAND_CLIPBOARD_HPP__

// Wayland clipboard backend: one-shot set (serves the first consumer, then drops ownership), bounded get

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/clipboard.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/time.hpp>

namespace sihd::sys::clipboard::wayland
{

inline constexpr sihd::util::Duration get_timeout(sihd::util::time::sec(1));

std::vector<RawContent> get_raw();
std::vector<RawContent> get_raw(const std::vector<std::string_view> & wanted_mimes);
bool set_raw(const std::vector<RawContentView> & contents, sihd::util::Timestamp deadline);

} // namespace sihd::sys::clipboard::wayland

#endif
