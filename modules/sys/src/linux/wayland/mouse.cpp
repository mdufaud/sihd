#include "mouse.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include <sihd/sys/mouse.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "../internal/deadline.hpp"
#include "connection.hpp"
#include "outputs.hpp"
#include "protocols/wlr-virtual-pointer-unstable-v1-client-protocol.h"

// Native zwlr_virtual_pointer_v1 injection: wlroots-style compositors only, the manager global gates it

using sihd::sys::internal::wayland::roundtrip_bounded;

using enum sihd::util::ErrorCode;

namespace sihd::sys::mouse::wayland
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::mouse::wayland");

namespace
{

constexpr Duration pointer_timeout = Duration(time::sec(2));

// One axis detent in the wayland axis convention.
constexpr double detent = 10.0;

// evdev button codes indexed by MouseButton: left, middle, right, side, extra.
constexpr auto button_codes = std::to_array<uint32_t>({0x110, 0x112, 0x111, 0x113, 0x114});

class Connection: public internal::wayland::Connection
{
    public:
        explicit Connection(Timestamp deadline): internal::wayland::Connection() { bind_globals(deadline); }

        wl_seat *seat = nullptr;
        zwlr_virtual_pointer_manager_v1 *manager = nullptr;

    protected:
        void bind_global(wl_registry *registry, uint32_t name, const char *interface, uint32_t version) override
        {
            if (strcmp(interface, wl_seat_interface.name) == 0 && seat == nullptr)
                seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
            else if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0 && manager == nullptr)
                manager = static_cast<zwlr_virtual_pointer_manager_v1 *>(
                    wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1));
            else
                _outputs.bind(registry, name, interface, version);
        }

        bool ready() const override { return seat != nullptr && manager != nullptr; }

    public:
        internal::wayland::Outputs & outputs() { return _outputs; }

    private:
        internal::wayland::Outputs _outputs;
};

class PointerSession
{
    public:
        explicit PointerSession(Timestamp deadline): _deadline(deadline), _conn(deadline) {}

        internal::wayland::Outputs & outputs() { return _conn.outputs(); }

        Result<void> start()
        {
            if (!_conn.ok())
                return std::unexpected(Error(io_error, "could not connect to the display"));

            _pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(_conn.manager, _conn.seat);
            if (_pointer == nullptr)
                return std::unexpected(Error(not_supported, "could not create the virtual pointer"));
            return {};
        }

        void motion(int32_t dx, int32_t dy)
        {
            zwlr_virtual_pointer_v1_motion(_pointer, 0, wl_fixed_from_int(dx), wl_fixed_from_int(dy));
        }

        void motion_absolute(int32_t x, int32_t y, uint32_t extent_x, uint32_t extent_y)
        {
            zwlr_virtual_pointer_v1_motion_absolute(_pointer, 0, x, y, extent_x, extent_y);
        }

        void button(uint32_t evdev_button, bool down)
        {
            zwlr_virtual_pointer_v1_button(_pointer,
                                           0,
                                           evdev_button,
                                           down ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
        }

        void scroll(int x, int y)
        {
            // axis value is positive scrolling down and right; `y` counts detents up
            if (y != 0)
            {
                zwlr_virtual_pointer_v1_axis(_pointer,
                                             0,
                                             WL_POINTER_AXIS_VERTICAL_SCROLL,
                                             wl_fixed_from_double(-detent * y));
                zwlr_virtual_pointer_v1_axis_discrete(_pointer,
                                                      0,
                                                      WL_POINTER_AXIS_VERTICAL_SCROLL,
                                                      wl_fixed_from_double(-detent),
                                                      -y);
            }
            if (x != 0)
            {
                zwlr_virtual_pointer_v1_axis(_pointer,
                                             0,
                                             WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                                             wl_fixed_from_double(detent * x));
                zwlr_virtual_pointer_v1_axis_discrete(_pointer,
                                                      0,
                                                      WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                                                      wl_fixed_from_double(detent),
                                                      x);
            }
            if (x != 0 || y != 0)
                zwlr_virtual_pointer_v1_frame(_pointer);
        }

        void frame() { zwlr_virtual_pointer_v1_frame(_pointer); }

        Result<void> finish()
        {
            if (!roundtrip_bounded(_conn, _deadline))
                return std::unexpected(Error(timeout, "the pointer events were not delivered in time"));
            return {};
        }

    private:
        Timestamp _deadline;
        Connection _conn;
        zwlr_virtual_pointer_v1 *_pointer = nullptr;
};

template <typename Body>
Result<void> pointer_gesture(const Body & body)
{
    PointerSession session(internal::deadline_from_now(pointer_timeout));
    auto res = session.start();
    SIHD_UNEXPECTED_RETURN(res)
    res = body(session);
    SIHD_UNEXPECTED_RETURN(res)
    res = session.finish();
    SIHD_UNEXPECTED_RETURN(res)
    return {};
}

} // namespace

Result<void> mouse_move(int x, int y)
{
    return pointer_gesture([&](PointerSession & session) -> Result<void> {
        const auto extents = session.outputs().extents();
        if (!extents.valid)
            return std::unexpected(Error(not_supported, "no output layout known"));
        const int clamped_x = std::clamp(x, extents.x, extents.x + extents.width - 1);
        const int clamped_y = std::clamp(y, extents.y, extents.y + extents.height - 1);
        session.motion_absolute(clamped_x - extents.x, clamped_y - extents.y, extents.width, extents.height);
        session.frame();
        return {};
    });
}

Result<void> mouse_move_by(int dx, int dy)
{
    return pointer_gesture([&](PointerSession & session) -> Result<void> {
        session.motion(dx, dy);
        session.frame();
        return {};
    });
}

Result<void> mouse_button(MouseButton button, bool down)
{
    return pointer_gesture([&](PointerSession & session) -> Result<void> {
        session.button(button_codes[std::to_underlying(button)], down);
        session.frame();
        return {};
    });
}

Result<void> mouse_click(MouseButton button)
{
    return pointer_gesture([&](PointerSession & session) -> Result<void> {
        const uint32_t code = button_codes[std::to_underlying(button)];
        session.button(code, true);
        session.button(code, false);
        session.frame();
        return {};
    });
}

Result<void> mouse_scroll(int x, int y)
{
    return pointer_gesture([&](PointerSession & session) -> Result<void> {
        session.scroll(x, y);
        return {};
    });
}

} // namespace sihd::sys::mouse::wayland
