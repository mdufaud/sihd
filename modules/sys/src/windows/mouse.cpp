#include <windows.h>

#include <cstdint>
#include <utility>
#include <vector>

#include <sihd/sys/mouse.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;

using namespace sihd::util;

namespace sihd::sys::mouse
{

SIHD_NEW_LOGGER("sihd::sys::mouse");

namespace
{

constexpr DWORD button_downs[] = {MOUSEEVENTF_LEFTDOWN,
                                  MOUSEEVENTF_MIDDLEDOWN,
                                  MOUSEEVENTF_RIGHTDOWN,
                                  MOUSEEVENTF_XDOWN,
                                  MOUSEEVENTF_XDOWN};
constexpr DWORD button_ups[] = {MOUSEEVENTF_LEFTUP,
                                MOUSEEVENTF_MIDDLEUP,
                                MOUSEEVENTF_RIGHTUP,
                                MOUSEEVENTF_XUP,
                                MOUSEEVENTF_XUP};
// The mouseData payload of the XBUTTON events, zero otherwise.
constexpr DWORD xbutton_data[] = {0, 0, 0, XBUTTON1, XBUTTON2};

INPUT mouse_input(DWORD flags, LONG dx = 0, LONG dy = 0, DWORD mouse_data = 0)
{
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.mouseData = mouse_data;
    return input;
}

Result<void> send(std::vector<INPUT> inputs)
{
    if (SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT)) != inputs.size())
        return std::unexpected(Error(io_error, "SendInput: {}", os::last_error_str()));
    return {};
}

} // namespace

Result<void> mouse_move(int x, int y)
{
    const LONG left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const LONG top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const LONG width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const LONG height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width <= 0 || height <= 0)
        return std::unexpected(Error(not_supported, "no virtual desktop"));

    const long long span_x = width > 1 ? width - 1 : 1;
    const long long span_y = height > 1 ? height - 1 : 1;
    const LONG normalized_x = static_cast<LONG>((x - left) * 65535LL / span_x);
    const LONG normalized_y = static_cast<LONG>((y - top) * 65535LL / span_y);
    return send(
        {mouse_input(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK, normalized_x, normalized_y)});
}

Result<void> mouse_move_by(int dx, int dy)
{
    return send({mouse_input(MOUSEEVENTF_MOVE, dx, dy)});
}

Result<void> mouse_button(MouseButton button, bool down)
{
    const size_t index = std::to_underlying(button);
    return send({mouse_input(down ? button_downs[index] : button_ups[index], 0, 0, xbutton_data[index])});
}

Result<void> mouse_click(MouseButton button)
{
    const size_t index = std::to_underlying(button);
    return send({mouse_input(button_downs[index], 0, 0, xbutton_data[index]),
                 mouse_input(button_ups[index], 0, 0, xbutton_data[index])});
}

Result<void> mouse_scroll(int x, int y)
{
    std::vector<INPUT> inputs;
    if (y != 0)
        inputs.push_back(mouse_input(MOUSEEVENTF_WHEEL, 0, 0, static_cast<DWORD>(y * WHEEL_DELTA)));
    if (x != 0)
        inputs.push_back(mouse_input(MOUSEEVENTF_HWHEEL, 0, 0, static_cast<DWORD>(x * WHEEL_DELTA)));
    if (inputs.empty())
        return {};
    return send(inputs);
}

} // namespace sihd::sys::mouse
