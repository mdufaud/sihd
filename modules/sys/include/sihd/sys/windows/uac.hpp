#ifndef __SIHD_SYS_WINDOWS_UAC_HPP__
#define __SIHD_SYS_WINDOWS_UAC_HPP__

#include <span>
#include <string>

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# error "sihd/sys/windows headers are windows only"
#endif

namespace sihd::sys::windows_::uac
{

// refusing the elevation prompt is permission_denied
sihd::util::Result<void> relaunch_elevated(std::span<const std::string> args);

} // namespace sihd::sys::windows_::uac

#endif
