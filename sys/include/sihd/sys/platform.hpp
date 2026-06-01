#ifndef __SIHD_SYS_PLATFORM_HPP__
#define __SIHD_SYS_PLATFORM_HPP__

#include <sihd/util/build.hpp>

// === Platform typedefs ===

#if !defined(__SIHD_WINDOWS__)
# include <sys/resource.h> // rlim_t
# include <sys/socket.h>   // socklen_t
#else
typedef int socklen_t;
typedef unsigned long rlim_t;
typedef unsigned int uid_t;
#endif

#endif
