#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

// sihd headers first: Xlib poisons common identifiers (None, Status, True,
// False, ...) with its macros.
#include <sihd/sys/keyboard.hpp>
#include <sihd/util/Logger.hpp>

#include "../internal/utf8.hpp"
#include "keyboard.hpp"
#include "x11_display.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

// XTest fake key events over the live keymap: letters and keys map to their
// keycode (shift column handled), characters the keymap cannot produce are
// typed through keycodes temporarily remapped to their unicode keysym

using enum sihd::util::ErrorCode;

namespace sihd::sys::keyboard::x11
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::keyboard");

namespace
{

// Keysyms indexed by Key, skipping Key::none.
constexpr KeySym key_keysyms[] = {
    XK_a,      XK_b,    XK_c,   XK_d,       XK_e,         XK_f,   XK_g,      XK_h,         XK_i,
    XK_j,      XK_k,    XK_l,   XK_m,       XK_n,         XK_o,   XK_p,      XK_q,         XK_r,
    XK_s,      XK_t,    XK_u,   XK_v,       XK_w,         XK_x,   XK_y,      XK_z,         XK_0,
    XK_1,      XK_2,    XK_3,   XK_4,       XK_5,         XK_6,   XK_7,      XK_8,         XK_9,
    XK_F1,     XK_F2,   XK_F3,  XK_F4,      XK_F5,        XK_F6,  XK_F7,     XK_F8,        XK_F9,
    XK_F10,    XK_F11,  XK_F12, XK_space,   XK_Return,    XK_Tab, XK_Escape, XK_BackSpace, XK_Delete,
    XK_Insert, XK_Home, XK_End, XK_Page_Up, XK_Page_Down, XK_Up,  XK_Down,   XK_Left,      XK_Right,
};

constexpr KeySym modifier_keysyms[] = {XK_Shift_L, XK_Control_L, XK_Alt_L, XK_Super_L};

static_assert(sizeof(key_keysyms) / sizeof(key_keysyms[0]) == std::to_underlying(Key::right),
              "the keysym table must cover every Key");

KeySym keysym_of(Key key)
{
    return key_keysyms[std::to_underlying(key) - 1];
}

struct Keycode
{
        KeyCode code = 0;
        bool shift = false;
};

// Server keymap snapshot; remapped keycodes are restored when it dies.
class Keymap
{
    public:
        explicit Keymap(Display *display): _display(display)
        {
            XDisplayKeycodes(display, &_min, &_max);
            _syms = XGetKeyboardMapping(display, _min, _max - _min + 1, &_per_keycode);
        }

        ~Keymap()
        {
            for (const int code : _remapped)
            {
                XChangeKeyboardMapping(_display, code, _per_keycode, original(code), 1);
            }
            if (_remapped.empty() == false)
                XSync(_display, False);
            XFree(_syms);
        }

        Keymap(const Keymap &) = delete;
        Keymap & operator=(const Keymap &) = delete;

        // An unshifted match wins over a shifted one.
        std::optional<Keycode> find(KeySym keysym) const
        {
            if (_syms == nullptr)
                return std::nullopt;
            for (int column = 0; column < 2; ++column)
            {
                for (int code = _min; code <= _max; ++code)
                {
                    const KeySym *syms = _syms + (code - _min) * _per_keycode;
                    if (_per_keycode > column && syms[column] == keysym)
                        return Keycode {static_cast<KeyCode>(code), column == 1};
                }
            }
            return std::nullopt;
        }

        // Remaps an unused keycode to `keysym` until the keymap dies.
        std::optional<KeyCode> remap(KeySym keysym)
        {
            if (_syms == nullptr)
                return std::nullopt;
            for (int code = _min; code <= _max; ++code)
            {
                if (std::ranges::contains(_remapped, code))
                    continue;
                bool free = true;
                for (int col = 0; free && col < _per_keycode; ++col)
                    free = original(code)[col] == NoSymbol;
                if (!free)
                    continue;
                KeySym pair[2] = {keysym, NoSymbol};
                XChangeKeyboardMapping(_display, code, 2, pair, 1);
                XSync(_display, False);
                _remapped.push_back(code);
                return static_cast<KeyCode>(code);
            }
            return std::nullopt;
        }

    private:
        KeySym *original(int code) { return _syms + (code - _min) * _per_keycode; }

        Display *_display = nullptr;
        KeySym *_syms = nullptr;
        int _min = 0;
        int _max = 0;
        int _per_keycode = 0;
        std::vector<int> _remapped;
};

bool fake_key(Display *display, KeyCode code, bool down)
{
    return XTestFakeKeyEvent(display, code, down ? True : False, CurrentTime) != 0;
}

std::optional<KeyCode> shift_keycode(Keymap & keymap, const Keycode & keycode)
{
    if (!keycode.shift)
        return KeyCode(0);
    auto shift = keymap.find(XK_Shift_L);
    if (!shift.has_value())
        return std::nullopt;
    return shift->code;
}

Result<void> tap_key(Display *display, Keymap & keymap, const Keycode & keycode)
{
    auto shift = shift_keycode(keymap, keycode);
    if (!shift.has_value())
        return std::unexpected(Error(not_supported, "no shift keycode in the keymap"));

    if (keycode.shift)
        fake_key(display, *shift, true);
    if (!fake_key(display, keycode.code, true) || !fake_key(display, keycode.code, false))
        return std::unexpected(Error(io_error, "could not fake the key event"));
    if (keycode.shift)
        fake_key(display, *shift, false);
    return {};
}

} // namespace

Result<void> type_text(std::string_view utf8_text)
{
    const auto codepoints = internal::utf8_decode(utf8_text);
    if (!codepoints.has_value())
        return std::unexpected(Error(invalid_argument, "invalid utf-8 text"));

    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());
    Display *display = conn.display;
    Keymap keymap(display);

    for (const char32_t cp : *codepoints)
    {
        std::optional<Keycode> keycode;
        KeySym keysym = static_cast<KeySym>(cp);
        if (cp == U'\n')
            keysym = XK_Return;
        else if (cp == U'\t')
            keysym = XK_Tab;
        else if (cp <= 0x1F)
            return std::unexpected(Error(invalid_argument, "untypeable control character"));
        else if (cp > 0x7F)
        {
            keysym = 0x1000000 | cp;
            auto remapped = keymap.remap(keysym);
            if (!remapped.has_value())
                return std::unexpected(Error(not_supported, "no free keycode for the character"));
            keycode = Keycode {*remapped, false};
        }
        else
        {
            keycode = keymap.find(keysym);
            if (!keycode.has_value())
                return std::unexpected(Error(not_supported, "character not in the keymap"));
        }

        auto res = tap_key(display, keymap, *keycode);
        SIHD_UNEXPECTED_RETURN(res)
    }
    XFlush(display);
    return {};
}

Result<void> key_press(Key key, Modifiers mods)
{
    if (key == Key::none)
        return std::unexpected(Error(invalid_argument, "no key"));

    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());
    Display *display = conn.display;
    Keymap keymap(display);

    const auto keycode = keymap.find(keysym_of(key));
    if (!keycode.has_value())
        return std::unexpected(Error(not_supported, "key not in the keymap"));

    std::vector<KeyCode> held;
    const bool wants_shift = std::to_underlying(mods & Modifiers::shift) != 0;
    if (keycode->shift && !wants_shift)
    {
        auto shift = shift_keycode(keymap, *keycode);
        if (!shift.has_value())
            return std::unexpected(Error(not_supported, "no shift keycode in the keymap"));
        fake_key(display, *shift, true);
        held.push_back(*shift);
    }
    for (unsigned int bit = 0; bit < std::size(modifier_keysyms); ++bit)
    {
        if (std::to_underlying(mods) & (1 << bit))
        {
            const auto mod = keymap.find(modifier_keysyms[bit]);
            if (!mod.has_value())
                return std::unexpected(Error(not_supported, "modifier not in the keymap"));
            fake_key(display, mod->code, true);
            held.push_back(mod->code);
        }
    }

    if (!fake_key(display, keycode->code, true) || !fake_key(display, keycode->code, false))
        return std::unexpected(Error(io_error, "could not fake the key event"));

    for (auto it = held.rbegin(); it != held.rend(); ++it)
        fake_key(display, *it, false);
    XFlush(display);
    return {};
}

Result<void> key_down(Key key)
{
    if (key == Key::none)
        return std::unexpected(Error(invalid_argument, "no key"));

    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());
    Keymap keymap(conn.display);

    const auto keycode = keymap.find(keysym_of(key));
    if (!keycode.has_value())
        return std::unexpected(Error(not_supported, "key not in the keymap"));
    if (!fake_key(conn.display, keycode->code, true))
        return std::unexpected(Error(io_error, "could not fake the key event"));
    XFlush(conn.display);
    return {};
}

Result<void> key_up(Key key)
{
    if (key == Key::none)
        return std::unexpected(Error(invalid_argument, "no key"));

    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());
    Keymap keymap(conn.display);

    const auto keycode = keymap.find(keysym_of(key));
    if (!keycode.has_value())
        return std::unexpected(Error(not_supported, "key not in the keymap"));
    if (!fake_key(conn.display, keycode->code, false))
        return std::unexpected(Error(io_error, "could not fake the key event"));
    XFlush(conn.display);
    return {};
}

} // namespace sihd::sys::keyboard::x11
