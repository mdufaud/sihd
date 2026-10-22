#ifndef __SIHD_SYS_LINUX_INTERNAL_DISPATCH_HPP__
#define __SIHD_SYS_LINUX_INTERNAL_DISPATCH_HPP__

#include <type_traits>

namespace sihd::sys::internal
{

// Calls the session-matching backend first, the other on failure; the session backend's error wins
template <typename Fn, typename... Args>
std::invoke_result_t<Fn, Args...> try_first(bool wayland_first, Fn wayland_fn, Fn x11_fn, Args &&...args)
{
    if (wayland_first)
    {
        std::invoke_result_t<Fn, Args...> res = wayland_fn(args...);
        if (res.has_value())
            return res;
        auto alt = x11_fn(args...);
        if (alt.has_value())
            return alt;
        return res;
    }
    std::invoke_result_t<Fn, Args...> res = x11_fn(args...);
    if (res.has_value())
        return res;
    auto alt = wayland_fn(args...);
    if (alt.has_value())
        return alt;
    return res;
}

} // namespace sihd::sys::internal

#endif
