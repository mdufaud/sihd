#include "../internal/desktop_env.hpp"
#include "../x11/clipboard.hpp"
#include "../x11/keyboard.hpp"
#include "../x11/mouse.hpp"
#include "../x11/screen.hpp"
#include "../x11/screenshot.hpp"

namespace sihd::sys
{

using namespace sihd::util;

namespace
{

Error not_compiled()
{
    return {ErrorCode::not_supported, "x11 backend not compiled in"};
}

} // namespace

namespace internal
{

bool x11_compiled()
{
    return false;
}

} // namespace internal

namespace clipboard::x11
{

std::vector<RawContent> get_raw()
{
    return {};
}

std::vector<RawContent> get_raw([[maybe_unused]] const std::vector<std::string_view> & wanted_mimes)
{
    return {};
}

bool set_raw([[maybe_unused]] const std::vector<RawContentView> & contents, [[maybe_unused]] Timestamp deadline)
{
    return false;
}

} // namespace clipboard::x11

namespace keyboard::x11
{

Result<void> type_text([[maybe_unused]] std::string_view utf8_text)
{
    return std::unexpected(not_compiled());
}

Result<void> key_press([[maybe_unused]] Key key, [[maybe_unused]] Modifiers mods)
{
    return std::unexpected(not_compiled());
}

Result<void> key_down([[maybe_unused]] Key key)
{
    return std::unexpected(not_compiled());
}

Result<void> key_up([[maybe_unused]] Key key)
{
    return std::unexpected(not_compiled());
}

} // namespace keyboard::x11

namespace mouse::x11
{

Result<void> mouse_move([[maybe_unused]] int x, [[maybe_unused]] int y)
{
    return std::unexpected(not_compiled());
}

Result<void> mouse_move_by([[maybe_unused]] int dx, [[maybe_unused]] int dy)
{
    return std::unexpected(not_compiled());
}

Result<void> mouse_button([[maybe_unused]] MouseButton button, [[maybe_unused]] bool down)
{
    return std::unexpected(not_compiled());
}

Result<void> mouse_click([[maybe_unused]] MouseButton button)
{
    return std::unexpected(not_compiled());
}

Result<void> mouse_scroll([[maybe_unused]] int x, [[maybe_unused]] int y)
{
    return std::unexpected(not_compiled());
}

} // namespace mouse::x11

namespace screen::x11
{

Result<std::vector<Monitor>> monitors()
{
    return std::unexpected(not_compiled());
}

} // namespace screen::x11

namespace screenshot::x11
{

bool take_screen([[maybe_unused]] Bitmap & bm)
{
    return false;
}

bool take_focused([[maybe_unused]] Bitmap & bm)
{
    return false;
}

bool take_under_cursor([[maybe_unused]] Bitmap & bm)
{
    return false;
}

bool take_window_name([[maybe_unused]] Bitmap & bm, [[maybe_unused]] std::string_view name)
{
    return false;
}

} // namespace screenshot::x11

} // namespace sihd::sys
