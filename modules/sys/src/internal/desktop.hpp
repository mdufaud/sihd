#ifndef __SIHD_SYS_INTERNAL_DESKTOP_HPP__
#define __SIHD_SYS_INTERNAL_DESKTOP_HPP__

#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::sys::internal
{

sihd::util::Result<void> desktop_open_uri(sihd::util::ZStringView uri);
sihd::util::Result<void> desktop_open_file(sihd::util::ZStringView path);
sihd::util::Result<void> desktop_reveal(sihd::util::ZStringView path);

} // namespace sihd::sys::internal

#endif
