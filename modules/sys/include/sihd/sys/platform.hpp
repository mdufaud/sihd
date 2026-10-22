#ifndef __SIHD_SYS_PLATFORM_HPP__
#define __SIHD_SYS_PLATFORM_HPP__

#include <sys/stat.h>

#include <sihd/util/build.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <sys/resource.h>
# include <sys/socket.h>
#else
# include <sys/types.h>
using socklen_t = int;
using rlim_t = unsigned long;
using uid_t = unsigned int;
using gid_t = unsigned int;
#endif

#endif
