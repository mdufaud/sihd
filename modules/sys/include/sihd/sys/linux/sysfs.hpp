#ifndef __SIHD_SYS_LINUX_SYSFS_HPP__
#define __SIHD_SYS_LINUX_SYSFS_HPP__

#include <cstdint>
#include <string>
#include <string_view>

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/sys/linux headers are linux only"
#endif

namespace sihd::sys::linux_::sysfs
{

sihd::util::Result<std::string> read(std::string_view path);

sihd::util::Result<uint64_t> read_uint(std::string_view path);

// unbuffered and never creates: the kernel rejects a value at write(), stdio would lose it at fclose
sihd::util::Result<void> write(std::string_view path, std::string_view value);

} // namespace sihd::sys::linux_::sysfs

#endif
