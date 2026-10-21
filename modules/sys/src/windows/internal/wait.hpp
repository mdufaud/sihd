#ifndef __SIHD_SYS_WINDOWS_INTERNAL_WAIT_HPP__
#define __SIHD_SYS_WINDOWS_INTERNAL_WAIT_HPP__

#include <windows.h>

#include <sihd/util/Duration.hpp>

namespace sihd::sys::internal
{

DWORD deadline_ms(sihd::util::Duration remaining);

} // namespace sihd::sys::internal

#endif
