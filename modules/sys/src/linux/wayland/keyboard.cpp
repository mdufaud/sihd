#include "keyboard.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <sihd/sys/keyboard.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "../internal/deadline.hpp"
#include "../internal/utf8.hpp"
#include "connection.hpp"
#include "protocols/zwp-virtual-keyboard-unstable-v1-client-protocol.h"

// linux/memfd.h is a kernel uapi header some libcs do not ship.
#ifndef MFD_CLOEXEC
# define MFD_CLOEXEC 0x0001U
#endif

using sihd::sys::internal::wayland::roundtrip_bounded;

using enum sihd::util::ErrorCode;

namespace sihd::sys::keyboard::wayland
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::keyboard::wayland");

namespace
{

constexpr Duration key_timeout = Duration(time::sec(2));

// xkb modifier masks of the complete compat set, indexed by the Modifiers bit.
constexpr auto modifier_masks = std::to_array<uint32_t>({1 << 0, 1 << 2, 1 << 3, 1 << 6});

constexpr auto key_names = std::to_array<std::string_view>({
    "a",         "b",      "c",      "d",    "e",   "f",     "g",    "h",   "i",    "j",     "k",      "l",   "m",
    "n",         "o",      "p",      "q",    "r",   "s",     "t",    "u",   "v",    "w",     "x",      "y",   "z",
    "0",         "1",      "2",      "3",    "4",   "5",     "6",    "7",   "8",    "9",     "F1",     "F2",  "F3",
    "F4",        "F5",     "F6",     "F7",   "F8",  "F9",    "F10",  "F11", "F12",  "space", "Return", "Tab", "Escape",
    "BackSpace", "Delete", "Insert", "Home", "End", "Prior", "Next", "Up",  "Down", "Left",  "Right",
});

static_assert(key_names.size() == std::to_underlying(Key::right), "the keysym name table must cover every Key");

std::string_view key_name(Key key)
{
    return key_names[std::to_underlying(key) - 1];
}

// ascii literals; else xkbcommon's unicode keysym 0x1000000 + codepoint; empty when untypeable
std::string codepoint_name(char32_t cp)
{
    if (cp == U'\n')
        return "Return";
    if (cp == U'\t')
        return "Tab";
    if (cp <= 0x1F)
        return {};
    if (cp <= 0x7F)
        return std::string(1, static_cast<char>(cp));
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%x", 0x1000000u | static_cast<uint32_t>(cp));
    return buf;
}

class Keymap
{
    public:
        uint32_t keycode_of(std::string_view keysym_name)
        {
            for (size_t i = 0; i < _entries.size(); ++i)
            {
                if (_entries[i] == keysym_name)
                    return first_keycode + static_cast<uint32_t>(i);
            }
            _entries.emplace_back(keysym_name);
            return first_keycode + static_cast<uint32_t>(_entries.size()) - 1;
        }

        std::string text() const
        {
            std::string out;
            out += "xkb_keymap {\n";
            out += "xkb_keycodes \"(unnamed)\" {\n";
            out += "minimum = 8;\n";
            out += "maximum = " + std::to_string(first_keycode + _entries.size()) + ";\n";

            for (size_t i = 0; i < _entries.size(); ++i)
            {
                out += "<K" + std::to_string(i + 1) + "> = " + std::to_string(first_keycode + i) + ";\n";
            }
            out += "};\n";
            out += "xkb_types \"(unnamed)\" { include \"complete\" };\n";
            out += "xkb_compatibility \"(unnamed)\" { include \"complete\" };\n";
            out += "xkb_symbols \"(unnamed)\" {\n";
            for (size_t i = 0; i < _entries.size(); ++i)
            {
                out += "key <K" + std::to_string(i + 1) + "> {[" + _entries[i] + "]};\n";
            }
            out += "};\n";
            out += "};\n";
            return out;
        }

    private:
        // The first custom keycode, below it xkb holds its minimum keycode 8.
        static constexpr uint32_t first_keycode = 9;

        std::vector<std::string> _entries;
};

class Connection: public sihd::sys::internal::wayland::Connection
{
    public:
        explicit Connection(Timestamp deadline): sihd::sys::internal::wayland::Connection() { bind_globals(deadline); }

        wl_seat *seat = nullptr;
        zwp_virtual_keyboard_manager_v1 *manager = nullptr;

    protected:
        void bind_global(wl_registry *registry, uint32_t name, const char *interface, uint32_t) override
        {
            if (strcmp(interface, wl_seat_interface.name) == 0 && seat == nullptr)
                seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
            else if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0 && manager == nullptr)
                manager = static_cast<zwp_virtual_keyboard_manager_v1 *>(
                    wl_registry_bind(registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1));
        }

        bool ready() const override { return seat != nullptr && manager != nullptr; }
};

class KeyboardSession
{
    public:
        explicit KeyboardSession(Timestamp deadline): _deadline(deadline), _conn(deadline) {}

        Result<void> start(const std::string & keymap_text)
        {
            if (!_conn.ok())
                return std::unexpected(Error(io_error, "could not connect to the display"));

            _keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(_conn.manager, _conn.seat);
            if (_keyboard == nullptr)
                return std::unexpected(Error(not_supported, "could not create the virtual keyboard"));

            const int fd = write_keymap(keymap_text);
            if (fd < 0)
                return std::unexpected(Error(io_error, "could not write the keymap"));
            Defer close_fd([&fd] { close(fd); });

            zwp_virtual_keyboard_v1_keymap(_keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, keymap_text.size() + 1);
            // the compositor takes the keymap fd on the roundtrip: only then may our copy close
            if (!roundtrip_bounded(_conn, _deadline))
                return std::unexpected(Error(timeout, "the keymap was not absorbed in time"));
            return {};
        }

        void tap(uint32_t keycode)
        {
            zwp_virtual_keyboard_v1_key(_keyboard, 0, keycode, WL_KEYBOARD_KEY_STATE_PRESSED);
            zwp_virtual_keyboard_v1_key(_keyboard, 0, keycode, WL_KEYBOARD_KEY_STATE_RELEASED);
        }

        void press(uint32_t keycode, bool down)
        {
            zwp_virtual_keyboard_v1_key(_keyboard,
                                        0,
                                        keycode,
                                        down ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
        }

        void modifiers(uint32_t mask) { zwp_virtual_keyboard_v1_modifiers(_keyboard, mask, 0, 0, 0); }

        Result<void> finish()
        {
            if (!roundtrip_bounded(_conn, _deadline))
                return std::unexpected(Error(timeout, "the keys were not delivered in time"));
            return {};
        }

    private:
        int write_keymap(const std::string & text)
        {
            const int fd = memfd_create("sihd-keymap", MFD_CLOEXEC);
            if (fd < 0)
                return -1;
            size_t written = 0;
            while (written < text.size())
            {
                const ssize_t bytes = ::write(fd, text.data() + written, text.size() - written);
                if (bytes <= 0 && errno != EINTR)
                {
                    close(fd);
                    return -1;
                }
                written += static_cast<size_t>(bytes);
            }
            return fd;
        }

        Timestamp _deadline;
        Connection _conn;
        zwp_virtual_keyboard_v1 *_keyboard = nullptr;
};

uint32_t modifiers_mask(Modifiers mods)
{
    uint32_t mask = 0;
    for (unsigned int bit = 0; bit < std::size(modifier_masks); ++bit)
    {
        if (std::to_underlying(mods) & (1 << bit))
            mask |= modifier_masks[bit];
    }
    return mask;
}

template <typename Body>
Result<void> keyboard_gesture(const Keymap & keymap, const Body & body)
{
    KeyboardSession session(internal::deadline_from_now(key_timeout));
    auto res = session.start(keymap.text());
    SIHD_UNEXPECTED_RETURN(res)
    body(session);
    res = session.finish();
    SIHD_UNEXPECTED_RETURN(res)
    return {};
}

} // namespace

Result<void> type_text(std::string_view utf8_text)
{
    const auto codepoints = internal::utf8_decode(utf8_text);
    if (!codepoints.has_value())
        return std::unexpected(Error(invalid_argument, "invalid utf-8 text"));

    Keymap keymap;
    std::vector<uint32_t> keycodes;
    keycodes.reserve(codepoints->size());
    for (const char32_t cp : *codepoints)
    {
        std::string name = codepoint_name(cp);
        if (name.empty())
            return std::unexpected(Error(invalid_argument, "untypeable control character"));
        keycodes.push_back(keymap.keycode_of(name));
    }

    return keyboard_gesture(keymap, [&](KeyboardSession & session) {
        for (const uint32_t keycode : keycodes)
            session.tap(keycode);
    });
}

Result<void> key_press(Key key, Modifiers mods)
{
    if (key == Key::none)
        return std::unexpected(Error(invalid_argument, "no key"));

    Keymap keymap;
    const uint32_t keycode = keymap.keycode_of(key_name(key));
    const uint32_t mask = modifiers_mask(mods);

    return keyboard_gesture(keymap, [&](KeyboardSession & session) {
        session.modifiers(mask);
        session.tap(keycode);
        session.modifiers(0);
    });
}

Result<void> key_down(Key key)
{
    if (key == Key::none)
        return std::unexpected(Error(invalid_argument, "no key"));

    Keymap keymap;
    const uint32_t keycode = keymap.keycode_of(key_name(key));

    return keyboard_gesture(keymap, [&](KeyboardSession & session) { session.press(keycode, true); });
}

Result<void> key_up(Key key)
{
    if (key == Key::none)
        return std::unexpected(Error(invalid_argument, "no key"));

    Keymap keymap;
    const uint32_t keycode = keymap.keycode_of(key_name(key));

    return keyboard_gesture(keymap, [&](KeyboardSession & session) { session.press(keycode, false); });
}

} // namespace sihd::sys::keyboard::wayland
