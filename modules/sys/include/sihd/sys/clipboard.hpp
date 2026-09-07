#ifndef __SIHD_SYS_CLIPBOARD_HPP__
#define __SIHD_SYS_CLIPBOARD_HPP__

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/mime.hpp>

namespace sihd::sys::clipboard
{

#if defined(SIHD_COMPILE_WITH_X11) || defined(SIHD_COMPILE_WITH_WAYLAND) || defined(__SIHD_WINDOWS__)
constexpr bool supported = true;
#else
constexpr bool supported = false;
#endif

struct RawContent
{
        std::string mime;
        sihd::util::ArrByte data;
};

struct RawContentView
{
        std::string_view mime;
        sihd::util::ArrByteView data;
};

std::vector<RawContent> get_raw();
std::vector<RawContent> get_raw(const std::vector<std::string_view> & wanted_mimes);

std::optional<std::string> to_text(const RawContent & raw);
std::optional<Bitmap> to_image(const RawContent & raw);

// can SIGPIPE
bool set(std::string_view text);
bool set(const Bitmap & bitmap);
bool set(std::string_view mime, sihd::util::ArrByteView data);
bool set_raw(const std::vector<RawContentView> & contents);

} // namespace sihd::sys::clipboard

#endif
