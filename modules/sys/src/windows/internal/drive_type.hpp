#ifndef __SIHD_SYS_WINDOWS_INTERNAL_DRIVE_TYPE_HPP__
#define __SIHD_SYS_WINDOWS_INTERNAL_DRIVE_TYPE_HPP__

#include <windows.h>

#include <sihd/sys/windows/volume.hpp>

namespace sihd::sys::internal
{

sihd::sys::windows_::volume::MediaType media_type_from_native(UINT drive_type);

} // namespace sihd::sys::internal

#endif
