#ifndef __SIHD_SYS_WINDOWS_INTERNAL_ENVIRONMENT_HPP__
#define __SIHD_SYS_WINDOWS_INTERNAL_ENVIRONMENT_HPP__

#include <string>

#include <sihd/sys/Environment.hpp>

namespace sihd::sys::internal
{

// win32 environment block: double-null-terminated, null-separated "KEY=VALUE",
// sorted case-insensitively by name
std::string to_windows_block(const Environment & env);

} // namespace sihd::sys::internal

#endif
