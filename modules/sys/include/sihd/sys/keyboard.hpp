#ifndef __SIHD_SYS_KEYBOARD_HPP__
#define __SIHD_SYS_KEYBOARD_HPP__

#include <cstdint>
#include <string_view>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::keyboard
{

#if defined(SIHD_COMPILE_WITH_X11) || defined(SIHD_COMPILE_WITH_WAYLAND) || defined(__SIHD_WINDOWS__)
constexpr bool supported = true;
#else
constexpr bool supported = false;
#endif

enum class Key : uint8_t
{
    none,
    a,
    b,
    c,
    d,
    e,
    f,
    g,
    h,
    i,
    j,
    k,
    l,
    m,
    n,
    o,
    p,
    q,
    r,
    s,
    t,
    u,
    v,
    w,
    x,
    y,
    z,
    digit0,
    digit1,
    digit2,
    digit3,
    digit4,
    digit5,
    digit6,
    digit7,
    digit8,
    digit9,
    f1,
    f2,
    f3,
    f4,
    f5,
    f6,
    f7,
    f8,
    f9,
    f10,
    f11,
    f12,
    space,
    enter,
    tab,
    escape,
    backspace,
    del,
    insert,
    home,
    end,
    page_up,
    page_down,
    up,
    down,
    left,
    right,
};

enum class Modifiers : uint8_t
{
    none = 0,
    shift = 1 << 0,
    ctrl = 1 << 1,
    alt = 1 << 2,
    meta = 1 << 3,
};

Modifiers operator|(Modifiers lhs, Modifiers rhs);

Modifiers operator&(Modifiers lhs, Modifiers rhs);

sihd::util::Result<void> type_text(std::string_view utf8_text);

sihd::util::Result<void> key_press(Key key, Modifiers mods = Modifiers::none);

sihd::util::Result<void> key_down(Key key);

sihd::util::Result<void> key_up(Key key);

} // namespace sihd::sys::keyboard

#endif
