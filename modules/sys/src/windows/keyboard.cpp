#include <windows.h>

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <sihd/sys/keyboard.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;

using namespace sihd::util;

namespace sihd::sys::keyboard
{

SIHD_NEW_LOGGER("sihd::sys::keyboard");

namespace
{

// Virtual key codes indexed by Key, skipping Key::none.
constexpr WORD key_codes[] = {
    'A',     'B',    'C',      'D',     'E',      'F',       'G',     'H',       'I',     'J',       'K',
    'L',     'M',    'N',      'O',     'P',      'Q',       'R',     'S',       'T',     'U',       'V',
    'W',     'X',    'Y',      'Z',     '0',      '1',       '2',     '3',       '4',     '5',       '6',
    '7',     '8',    '9',      VK_F1,   VK_F2,    VK_F3,     VK_F4,   VK_F5,     VK_F6,   VK_F7,     VK_F8,
    VK_F9,   VK_F10, VK_F11,   VK_F12,  VK_SPACE, VK_RETURN, VK_TAB,  VK_ESCAPE, VK_BACK, VK_DELETE, VK_INSERT,
    VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_UP,    VK_DOWN,   VK_LEFT, VK_RIGHT,
};

constexpr WORD modifier_codes[] = {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN};

static_assert(sizeof(key_codes) / sizeof(key_codes[0]) == std::to_underlying(Key::right),
              "the virtual key table must cover every Key");

WORD key_code_of(Key key)
{
    return key_codes[std::to_underlying(key) - 1];
}

// The navigation, editing and arrow keys carry the extended flag.
bool extended_key(Key key)
{
    return std::to_underlying(key) >= std::to_underlying(Key::del);
}

INPUT key_input(WORD code, bool down, bool extended = false)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = code;
    input.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
    return input;
}

Result<void> send(std::vector<INPUT> inputs)
{
    if (SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT)) != inputs.size())
        return std::unexpected(Error(io_error, "SendInput: {}", os::last_error_str()));
    return {};
}

} // namespace

Result<void> type_text(std::string_view utf8_text)
{
    if (utf8_text.empty())
        return {};
    // utf-16 units typed as unicode scan codes: surrogates ride along as two
    // units and every layout produces the same characters.
    const int size = MultiByteToWideChar(CP_UTF8,
                                         MB_ERR_INVALID_CHARS,
                                         utf8_text.data(),
                                         static_cast<int>(utf8_text.size()),
                                         nullptr,
                                         0);
    if (size <= 0)
        return std::unexpected(Error(invalid_argument, "invalid utf-8 text"));
    std::vector<wchar_t> units(size);
    if (MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            utf8_text.data(),
                            static_cast<int>(utf8_text.size()),
                            units.data(),
                            size)
        != size)
        return std::unexpected(Error(invalid_argument, "invalid utf-8 text"));

    std::vector<INPUT> inputs;
    inputs.reserve(units.size() * 2);
    for (const wchar_t unit : units)
    {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = unit;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(down);
        inputs.push_back(up);
    }
    return send(inputs);
}

Result<void> key_press(Key key, Modifiers mods)
{
    if (key == Key::none)
        return std::unexpected(Error(invalid_argument, "no key"));

    const WORD code = key_code_of(key);
    const bool extended = extended_key(key);

    std::vector<INPUT> inputs;
    std::vector<WORD> held;
    inputs.reserve(2 + 2 * std::size(modifier_codes));
    for (unsigned int bit = 0; bit < std::size(modifier_codes); ++bit)
    {
        if (std::to_underlying(mods) & (1 << bit))
        {
            held.push_back(modifier_codes[bit]);
            inputs.push_back(key_input(modifier_codes[bit], true));
        }
    }
    inputs.push_back(key_input(code, true, extended));
    inputs.push_back(key_input(code, false, extended));
    for (auto it = held.rbegin(); it != held.rend(); ++it)
        inputs.push_back(key_input(*it, false));
    return send(inputs);
}

Result<void> key_down(Key key)
{
    if (key == Key::none)
        return std::unexpected(Error(invalid_argument, "no key"));
    return send({key_input(key_code_of(key), true, extended_key(key))});
}

Result<void> key_up(Key key)
{
    if (key == Key::none)
        return std::unexpected(Error(invalid_argument, "no key"));
    return send({key_input(key_code_of(key), false, extended_key(key))});
}

} // namespace sihd::sys::keyboard
