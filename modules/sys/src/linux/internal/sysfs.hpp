#ifndef __SIHD_SYS_LINUX_SYSFS_HPP__
#define __SIHD_SYS_LINUX_SYSFS_HPP__

#include <string>
#include <string_view>

#include <sihd/util/Result.hpp>

namespace sihd::sys::internal
{

sihd::util::Result<std::string> sysfs_read(std::string_view path);

// unbuffered and never creates: the kernel rejects a value at write(), stdio would lose it at fclose
sihd::util::Result<void> sysfs_write(std::string_view path, std::string_view value);

} // namespace sihd::sys::internal

#endif
