#ifndef __SIHD_SYS_PLATFORM_HPP__
#define __SIHD_SYS_PLATFORM_HPP__

#include <sys/stat.h>

#include <cstdint>

#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <sys/resource.h>
# include <sys/socket.h>
#else
# include <sys/types.h>
using socklen_t = int;
using uid_t = unsigned int;
using gid_t = unsigned int;
#endif

namespace sihd::sys
{

// public api never exposes rlim_t: its width follows the _FILE_OFFSET_BITS of each 32-bit consumer
using ResourceLimit = uint64_t;

} // namespace sihd::sys

#endif
