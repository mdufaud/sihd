// sihd headers first: Xlib poisons common identifiers (None, Status, True, False) with macros
#include "mouse.hpp"

#include <array>

#include <sihd/sys/mouse.hpp>
#include <sihd/util/Logger.hpp>

#include "x11_display.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

using enum sihd::util::ErrorCode;

namespace sihd::sys::mouse::x11
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::mouse::x11");

namespace
{

// Pointer buttons indexed by MouseButton: left, middle, right, back, forward.
constexpr auto button_codes = std::to_array<unsigned int>({1, 2, 3, 8, 9});

// Wheel buttons: up, down, left, right.
constexpr unsigned int scroll_up = 4;
constexpr unsigned int scroll_down = 5;
constexpr unsigned int scroll_left = 6;
constexpr unsigned int scroll_right = 7;

bool fake_button(Display *display, unsigned int button, bool down)
{
    return XTestFakeButtonEvent(display, button, down ? True : False, CurrentTime) != 0;
}

Result<void> fake_click(Display *display, unsigned int button)
{
    if (!fake_button(display, button, true) || !fake_button(display, button, false))
        return std::unexpected(Error(io_error, "could not fake the button event"));
    XFlush(display);
    return {};
}

} // namespace

Result<void> mouse_move(int x, int y)
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());
    if (XTestFakeMotionEvent(conn.display, -1, x, y, CurrentTime) == 0)
        return std::unexpected(Error(io_error, "could not move the cursor"));
    XFlush(conn.display);
    return {};
}

Result<void> mouse_move_by(int dx, int dy)
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());
    if (XTestFakeRelativeMotionEvent(conn.display, dx, dy, CurrentTime) == 0)
        return std::unexpected(Error(io_error, "could not move the cursor"));
    XFlush(conn.display);
    return {};
}

Result<void> mouse_button(MouseButton button, bool down)
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());
    if (!fake_button(conn.display, button_codes[std::to_underlying(button)], down))
        return std::unexpected(Error(io_error, "could not fake the button event"));
    XFlush(conn.display);
    return {};
}

Result<void> mouse_click(MouseButton button)
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());
    return fake_click(conn.display, button_codes[std::to_underlying(button)]);
}

Result<void> mouse_scroll(int x, int y)
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());

    const unsigned int vertical = y > 0 ? scroll_up : scroll_down;
    const unsigned int horizontal = x > 0 ? scroll_right : scroll_left;
    for (int i = 0; i < (y > 0 ? y : -y); ++i)
    {
        auto res = fake_click(conn.display, vertical);
        SIHD_UNEXPECTED_RETURN(res)
    }
    for (int i = 0; i < (x > 0 ? x : -x); ++i)
    {
        auto res = fake_click(conn.display, horizontal);
        SIHD_UNEXPECTED_RETURN(res)
    }
    XFlush(conn.display);
    return {};
}

} // namespace sihd::sys::mouse::x11
