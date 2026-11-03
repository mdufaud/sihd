#ifndef __SIHD_SYS_DESKTOP_HPP__
#define __SIHD_SYS_DESKTOP_HPP__

#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys::desktop
{

constexpr bool supported = (sihd::util::build::is_linux && !sihd::util::build::is_android
                            && !sihd::util::build::is_emscripten)
                           || sihd::util::build::is_windows;

enum class ColorScheme
{
    no_preference,
    dark,
    light
};

// in the default application of its scheme, a file:// uri goes through open_file
sihd::util::Result<void> open_uri(sihd::util::ZStringView uri);
sihd::util::Result<void> open_file(sihd::util::ZStringView path);
sihd::util::Result<void> reveal(sihd::util::ZStringView path);

sihd::util::Result<ColorScheme> color_scheme();

} // namespace sihd::sys::desktop

#endif
