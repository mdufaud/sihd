#ifndef __SIHD_UTIL_VERSION_HPP__
#define __SIHD_UTIL_VERSION_HPP__

#include <sihd/util/macro.hpp>

#if !defined(SIHD_VERSION_MAJOR)
# define SIHD_VERSION_MAJOR 0
#endif

#if !defined(SIHD_VERSION_MINOR)
# define SIHD_VERSION_MINOR 0
#endif

#if !defined(SIHD_VERSION_PATCH)
# define SIHD_VERSION_PATCH 0
#endif

#define SIHD_VERSION_STRING                                                                                            \
 __SIHD_STRINGIFY__(SIHD_VERSION_MAJOR)                                                                                \
 "." __SIHD_STRINGIFY__(SIHD_VERSION_MINOR) "." __SIHD_STRINGIFY__(SIHD_VERSION_PATCH)
// 2.34.76 -> 203476
#define SIHD_VERSION_NUM ((SIHD_VERSION_MAJOR * 100000) + (SIHD_VERSION_MINOR * 100) + (SIHD_VERSION_PATCH))

#endif