#ifndef __SIHD_SYS_X11_CLIPBOARD_HPP__
#define __SIHD_SYS_X11_CLIPBOARD_HPP__

// X11 clipboard backend of the dispatcher in src/linux/clipboard.cpp.
// One-shot set (serves the first consumer, then drops ownership); bounded get.
// The set deadline is computed once by the dispatcher and passed down.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/clipboard.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/time.hpp>

namespace sihd::sys::clipboard::x11
{

inline constexpr sihd::util::Duration get_timeout = sihd::util::time::sec(1);

#if defined(SIHD_COMPILE_WITH_X11)

// Every representation the current clipboard content is offered as.
std::vector<RawContent> get_raw();

// Same, restricted to the wanted mime types, fetched in `wanted_mimes` order.
std::vector<RawContent> get_raw(const std::vector<std::string_view> & wanted_mimes);

// Offer several representations of one content at once, until the deadline.
bool set_raw(const std::vector<RawContentView> & contents, sihd::util::Timestamp deadline);

#else

// The backend is not compiled in: inert inline fallbacks.

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

} // namespace sihd::sys::clipboard::x11

#endif
