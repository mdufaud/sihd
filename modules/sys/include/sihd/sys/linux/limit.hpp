#ifndef __SIHD_SYS_LINUX_LIMIT_HPP__
#define __SIHD_SYS_LINUX_LIMIT_HPP__

#include <limits>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/sys/linux headers are linux only"
#endif

namespace sihd::sys::linux_::limit
{

constexpr bool supported = sihd::util::build::is_linux && !sihd::util::build::is_emscripten;

constexpr rlim_t infinity = std::numeric_limits<rlim_t>::max();

enum class Resource
{
    as,
    core,
    cpu,
    data,
    fsize,
    locks,
    memlock,
    msgqueue,
    nice,
    nofile,
    nproc,
    rss,
    rtprio,
    sigpending,
    stack,
};

struct Limit
{
        rlim_t current = 0;
        rlim_t maximum = 0;
};

sihd::util::Result<Limit> get(Resource resource);
sihd::util::Result<void> set(Resource resource, rlim_t current, rlim_t maximum);

} // namespace sihd::sys::linux_::limit

#endif
