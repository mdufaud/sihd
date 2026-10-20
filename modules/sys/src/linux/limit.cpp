#include <sihd/sys/linux/limit.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
# include <sys/resource.h>
#endif

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::linux_::limit
{

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)

static_assert(infinity == RLIM_INFINITY);

namespace
{

// glibc types the resource as an enum in C++, musl and bionic as int
using NativeResource = decltype(RLIMIT_NOFILE);

NativeResource native_resource(Resource resource)
{
    switch (resource)
    {
        case Resource::as:
            return RLIMIT_AS;
        case Resource::core:
            return RLIMIT_CORE;
        case Resource::cpu:
            return RLIMIT_CPU;
        case Resource::data:
            return RLIMIT_DATA;
        case Resource::fsize:
            return RLIMIT_FSIZE;
        case Resource::locks:
            return RLIMIT_LOCKS;
        case Resource::memlock:
            return RLIMIT_MEMLOCK;
        case Resource::msgqueue:
            return RLIMIT_MSGQUEUE;
        case Resource::nice:
            return RLIMIT_NICE;
        case Resource::nofile:
            return RLIMIT_NOFILE;
        case Resource::nproc:
            return RLIMIT_NPROC;
        case Resource::rss:
            return RLIMIT_RSS;
        case Resource::rtprio:
            return RLIMIT_RTPRIO;
        case Resource::sigpending:
            return RLIMIT_SIGPENDING;
        case Resource::stack:
            return RLIMIT_STACK;
    }
    return static_cast<NativeResource>(-1);
}

} // namespace

Result<Limit> get(Resource resource)
{
    struct rlimit native = {};
    if (getrlimit(native_resource(resource), &native) == -1)
        return std::unexpected(Error::from_errno("could not getrlimit"));
    return Limit {native.rlim_cur, native.rlim_max};
}

Result<void> set(Resource resource, rlim_t current, rlim_t maximum)
{
    const struct rlimit native = {current, maximum};
    if (setrlimit(native_resource(resource), &native) == -1)
        return std::unexpected(Error::from_errno("could not setrlimit"));
    return {};
}

#else

Result<Limit> get(Resource)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<void> set(Resource, rlim_t, rlim_t)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

#endif

} // namespace sihd::sys::linux_::limit
