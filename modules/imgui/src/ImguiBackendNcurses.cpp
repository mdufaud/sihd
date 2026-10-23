#include <imgui.h>
#include <imgui_internal.h>

// wide-char api: glibc g++ gets it from _GNU_SOURCE, musl needs it asked for
#define NCURSES_WIDECHAR 1
#include <ncursesw/ncurses.h>
// ncurses may define `bool` as a macro (musl cross): undef it before std/imgui headers
#undef bool
#include <sys/stat.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <sihd/imgui/ImguiBackendNcurses.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>

using namespace sihd::util;

namespace sihd::imgui
{

SIHD_LOGGER;

namespace
{

ImGuiKey _ncurses_key_to_imgui(int key)
{
    switch (key)
    {
        case KEY_UP:
            return ImGuiKey_UpArrow;
        case KEY_DOWN:
            return ImGuiKey_DownArrow;
        case KEY_LEFT:
            return ImGuiKey_LeftArrow;
        case KEY_RIGHT:
            return ImGuiKey_RightArrow;
        case KEY_HOME:
            return ImGuiKey_Home;
        case KEY_END:
            return ImGuiKey_End;
        case KEY_PPAGE:
            return ImGuiKey_PageUp;
        case KEY_NPAGE:
            return ImGuiKey_PageDown;
        case KEY_IC:
            return ImGuiKey_Insert;
        case KEY_DC:
            return ImGuiKey_Delete;
        case KEY_BACKSPACE:
        case 8:
        case 127:
            return ImGuiKey_Backspace;
        case KEY_ENTER:
        case '\n':
        case '\r':
            return ImGuiKey_Enter;
        case '\t':
            return ImGuiKey_Tab;
        case 27:
            return ImGuiKey_Escape;
        case KEY_F(1):
            return ImGuiKey_F1;
        case KEY_F(2):
            return ImGuiKey_F2;
        case KEY_F(3):
            return ImGuiKey_F3;
        case KEY_F(4):
            return ImGuiKey_F4;
        case KEY_F(5):
            return ImGuiKey_F5;
        case KEY_F(6):
            return ImGuiKey_F6;
        case KEY_F(7):
            return ImGuiKey_F7;
        case KEY_F(8):
            return ImGuiKey_F8;
        case KEY_F(9):
            return ImGuiKey_F9;
        case KEY_F(10):
            return ImGuiKey_F10;
        case KEY_F(11):
            return ImGuiKey_F11;
        case KEY_F(12):
            return ImGuiKey_F12;
        default:
            return ImGuiKey_None;
    }
}

ImGuiKey _ncurses_shifted_key_to_imgui(int key)
{
    switch (key)
    {
        case KEY_SLEFT:
            return ImGuiKey_LeftArrow;
        case KEY_SRIGHT:
            return ImGuiKey_RightArrow;
        case KEY_SR:
            return ImGuiKey_UpArrow;
        case KEY_SF:
            return ImGuiKey_DownArrow;
        case KEY_SHOME:
            return ImGuiKey_Home;
        case KEY_SEND:
            return ImGuiKey_End;
        case KEY_SPREVIOUS:
            return ImGuiKey_PageUp;
        case KEY_SNEXT:
            return ImGuiKey_PageDown;
        case KEY_SIC:
            return ImGuiKey_Insert;
        case KEY_SDC:
            return ImGuiKey_Delete;
        case KEY_BTAB:
            return ImGuiKey_Tab;
        default:
            return ImGuiKey_None;
    }
}

ImGuiKey _ctrl_letter_to_imgui(int key)
{
    return (key >= 1 && key <= 26) ? (ImGuiKey)(ImGuiKey_A + key - 1) : ImGuiKey_None;
}

constexpr int escape_key = 27;
// define_key codes past every terminfo key: ncurses numbers extended keys from KEY_MAX upwards
constexpr int paste_start_key = KEY_MAX + 0x1000;
constexpr int paste_end_key = KEY_MAX + 0x1001;

struct ModifiableKey
{
        const char *capability;
        ImGuiKey key;
};

// terminfo names a modified key after its xterm parameter: kLFT5 is "\E[1;5D", Ctrl+Left
constexpr std::array<ModifiableKey, 10> modifiable_keys = {{
    {"kUP", ImGuiKey_UpArrow},
    {"kDN", ImGuiKey_DownArrow},
    {"kLFT", ImGuiKey_LeftArrow},
    {"kRIT", ImGuiKey_RightArrow},
    {"kHOM", ImGuiKey_Home},
    {"kEND", ImGuiKey_End},
    {"kPRV", ImGuiKey_PageUp},
    {"kNXT", ImGuiKey_PageDown},
    {"kIC", ImGuiKey_Insert},
    {"kDC", ImGuiKey_Delete},
}};

// xterm modifier parameter = 1 + (shift | alt << 1 | ctrl << 2)
ImGuiKeyChord xterm_modifiers(int parameter)
{
    const int bits = parameter - 1;
    return ((bits & 1) != 0 ? ImGuiMod_Shift : 0) | ((bits & 2) != 0 ? ImGuiMod_Alt : 0)
           | ((bits & 4) != 0 ? ImGuiMod_Ctrl : 0);
}

// Alt+letter and Alt+digit are shortcuts: the character itself is not typed
ImGuiKeyChord alt_character_chord(int key)
{
    if (key >= 'a' && key <= 'z')
        return ImGuiMod_Alt | (ImGuiKey_A + key - 'a');
    if (key >= 'A' && key <= 'Z')
        return ImGuiMod_Alt | ImGuiMod_Shift | (ImGuiKey_A + key - 'A');
    if (key >= '0' && key <= '9')
        return ImGuiMod_Alt | (ImGuiKey_0 + key - '0');
    return ImGuiKey_None;
}

// COLORTERM announces a 24-bit terminal whose TERM still names the 256-colour terminfo
bool open_screen()
{
    constexpr std::string_view palette_suffix = "-256color";
    const std::optional<std::string> colorterm = sihd::sys::env::get("COLORTERM");
    const std::optional<std::string> term = sihd::sys::env::get("TERM");
    if ((colorterm == "truecolor" || colorterm == "24bit") && term.has_value() && term->ends_with(palette_suffix))
    {
        const std::string direct = term->substr(0, term->size() - palette_suffix.size()) + "-direct";
        if (newterm(direct.c_str(), stdout, stdin) != nullptr)
            return true;
    }
    return initscr() != nullptr;
}

// tigetstr answers (char *)-1 for a name this terminfo does not hold as a string
const char *terminfo_string(const char *name)
{
    const char *value = tigetstr(name);
    return (value == nullptr || value == (const char *)-1) ? nullptr : value;
}

// imgui places the IME window one unit left of the text input caret
constexpr int ime_caret_offset = 1;
struct NcursesButton
{
        mmask_t press;
        mmask_t release;
        // ncurses merges a press and a release read together into a click, even with mouseinterval(0)
        mmask_t clicks;
};

// in imgui's order: left, right, middle
constexpr std::array<NcursesButton, 3> ncurses_buttons = {{
    {BUTTON1_PRESSED, BUTTON1_RELEASED, BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON1_TRIPLE_CLICKED},
    {BUTTON3_PRESSED, BUTTON3_RELEASED, BUTTON3_CLICKED | BUTTON3_DOUBLE_CLICKED | BUTTON3_TRIPLE_CLICKED},
    {BUTTON2_PRESSED, BUTTON2_RELEASED, BUTTON2_CLICKED | BUTTON2_DOUBLE_CLICKED | BUTTON2_TRIPLE_CLICKED},
}};
// imgui's tooltip and double-click delays and app animations run on frames: full rate this long after an input or a
// change
constexpr std::chrono::seconds active_after_activity(1);
constexpr double default_idle_frequency = 10.0;

// at a 1-cell font imgui hit-tests the resize grip only outside the window corner: move a press on the drawn grip there
ImVec2 _resize_grip_offset(int x, int y)
{
    ImGuiWindow *hovered = nullptr;
    ImGui::FindHoveredWindowEx(ImVec2((float)x, (float)y), true, &hovered, nullptr);
    if (hovered == nullptr)
        return ImVec2(0.0f, 0.0f);
    const ImGuiWindow *window = hovered->RootWindow;
    if (window->Collapsed || (window->Flags & (ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)))
        return ImVec2(0.0f, 0.0f);
    const float dx = window->Pos.x + window->Size.x - (float)x;
    const float dy = window->Pos.y + window->Size.y - (float)y;
    // the grip cell and the border cells beside it
    if (dx < 1.0f || dx > 2.0f || dy < 1.0f || dy > 2.0f)
        return ImVec2(0.0f, 0.0f);
    return ImVec2(dx, dy);
}

} // namespace

ImguiBackendNcurses::ImguiBackendNcurses():
    _is_init(false),
    _should_close(false),
    _mouse_seen(false),
    _ctrl_from_mouse(false),
    _ime_visible(false),
    _in_paste(false),
    _mx(0),
    _my(0),
    _grip_dx(0.0f),
    _grip_dy(0.0f),
    _ime_x(0),
    _ime_y(0),
    _cell_size(8.0f, 16.0f),
    _mstate(0),
    _idle_frequency(default_idle_frequency)
{
}

ImguiBackendNcurses::~ImguiBackendNcurses()
{
    this->shutdown();
}

bool ImguiBackendNcurses::init()
{
    if (_is_init)
        return true;

    // vcpkg's ncurses has a compiled-in terminfo prefix that is empty at runtime: add the system dirs
    if (!sihd::sys::env::get("TERMINFO").has_value())
    {
        static const char *candidates[] = {
            "/usr/share/terminfo",
            "/usr/lib/terminfo",
            "/etc/terminfo",
            nullptr,
        };
        std::string dirs;
        if (const std::optional<std::string> existing = sihd::sys::env::get("TERMINFO_DIRS"))
            dirs = *existing;
        for (const char **p = candidates; *p; ++p)
        {
            struct stat st;
            if (stat(*p, &st) == 0 && S_ISDIR(st.st_mode))
            {
                if (!dirs.empty())
                    dirs += ':';
                dirs += *p;
            }
        }
        if (!dirs.empty())
            SIHD_UNEXPECTED_LOG(sihd::sys::env::set("TERMINFO_DIRS", dirs));
    }

    if (!open_screen())
    {
        SIHD_LOG(error, "ImguiBackendNcurses: cannot open the terminal");
        return false;
    }
    cbreak(); // cooked mode: signals (Ctrl+C) still work, unlike raw()
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    wtimeout(stdscr, 0);
    set_escdelay(25);
    curs_set(0);
    mouseinterval(0); // disable click-detection delay: critical for drag
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
    // xterm mode 1003: report every cell movement, needed for window drag
    putp("\033[?1003h");
    this->_load_terminal_keys();
    fflush(stdout);

    ImGuiIO & io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_ncurses";
    io.KeyRepeatDelay = 0.050f;
    io.KeyRepeatRate = 0.050f;
    // apply the whole input queue every frame: mode-1003 motion floods would grow the deferred queue
    io.ConfigInputTrickleEventQueue = false;
    int sx = 0, sy = 0;
    getmaxyx(stdscr, sy, sx);
    io.DisplaySize = {(float)sx * _cell_size.x, (float)sy * _cell_size.y};

    ImGuiPlatformIO & platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_ImeUserData = this;
    platform_io.Platform_SetImeDataFn = &ImguiBackendNcurses::_set_ime_data;
    platform_io.Platform_ClipboardUserData = this;
    platform_io.Platform_GetClipboardTextFn = &ImguiBackendNcurses::_get_clipboard_text;
    platform_io.Platform_SetClipboardTextFn = &ImguiBackendNcurses::_set_clipboard_text;

    _last_time = std::chrono::steady_clock::now();
    _frame_start = _last_time;
    _last_activity = _last_time;
    _is_init = true;
    return true;
}

void ImguiBackendNcurses::_load_terminal_keys()
{
    _modified_keys.clear();
    for (const ModifiableKey & modifiable : modifiable_keys)
    {
        for (int parameter = 2; parameter <= 8; ++parameter)
        {
            const std::string capability = modifiable.capability + std::to_string(parameter);
            const char *sequence = terminfo_string(capability.c_str());
            const int code = sequence != nullptr ? key_defined(sequence) : 0;
            if (code > 0)
                _modified_keys[code] = modifiable.key | xterm_modifiers(parameter);
        }
    }

    // bracketed paste: the terminal frames pasted text with PS and PE, read back as two key codes
    const char *paste_enable = terminfo_string("BE");
    const char *paste_start = terminfo_string("PS");
    const char *paste_end = terminfo_string("PE");
    if (paste_enable != nullptr && paste_start != nullptr && paste_end != nullptr)
    {
        define_key(paste_start, paste_start_key);
        define_key(paste_end, paste_end_key);
        putp(paste_enable);
    }
}

void ImguiBackendNcurses::set_cell_size(ImVec2 cell_size)
{
    _cell_size = cell_size;
}

bool ImguiBackendNcurses::_virtual_pixels() const
{
    return _cell_size.x != 1.0f || _cell_size.y != 1.0f;
}

bool ImguiBackendNcurses::set_idle_frequency(double hz)
{
    if (hz <= 0.0)
        return false;
    _idle_frequency = hz;
    return true;
}

int ImguiBackendNcurses::_idle_timeout_ms() const
{
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (now - _last_activity < active_after_activity)
        return 0;
    const std::chrono::duration<double> idle_period(1.0 / _idle_frequency);
    const auto next_frame = _frame_start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(idle_period);
    const auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(next_frame - now);
    return (int)std::max<std::chrono::milliseconds::rep>(timeout.count(), 0);
}

bool ImguiBackendNcurses::should_close()
{
    return _should_close;
}

void ImguiBackendNcurses::_release_button(ImGuiIO & io, int button)
{
    _buttons[(size_t)button].down = false;
    if (button == ImGuiMouseButton_Left)
    {
        _grip_dx = 0.0f;
        _grip_dy = 0.0f;
    }
    io.AddMouseButtonEvent(button, false);
}

void ImguiBackendNcurses::_process_mouse_event(ImGuiIO & io)
{
    MEVENT ev;
    if (getmouse(&ev) != OK)
        return;

    _mx = ev.x;
    _my = ev.y;
    _mstate = ev.bstate;
    _mouse_seen = true;

    // a release after a same-poll press is deferred one poll so imgui registers the click
    for (int i = 0; i < (int)ncurses_buttons.size(); ++i)
    {
        const NcursesButton & ncurses_button = ncurses_buttons[(size_t)i];
        MouseButton & button = _buttons[(size_t)i];
        if (_mstate & (ncurses_button.press | ncurses_button.clicks))
        {
            button.pressed_this_poll = true;
            if (!button.down)
            {
                button.down = true;
                // with virtual pixels imgui's grip zone spans whole cells: no shift needed
                if (i == ImGuiMouseButton_Left && !this->_virtual_pixels())
                {
                    const ImVec2 grip_offset = _resize_grip_offset(_mx, _my);
                    _grip_dx = grip_offset.x;
                    _grip_dy = grip_offset.y;
                }
                io.AddMouseButtonEvent(i, true);
            }
        }
        if (_mstate & (ncurses_button.release | ncurses_button.clicks))
        {
            if (button.pressed_this_poll)
                button.pending_release = true;
            else if (button.down)
                this->_release_button(io, i);
        }
    }

    if (_mstate & BUTTON4_PRESSED)
        io.AddMouseWheelEvent(0.0f, 1.0f);
    if (_mstate & BUTTON5_PRESSED)
        io.AddMouseWheelEvent(0.0f, -1.0f);

    const bool new_ctrl = (_mstate & BUTTON_CTRL) != 0;
    if (new_ctrl != _ctrl_from_mouse)
    {
        _ctrl_from_mouse = new_ctrl;
        io.AddKeyEvent(ImGuiMod_Ctrl, new_ctrl);
        io.AddKeyEvent(ImGuiKey_LeftCtrl, new_ctrl);
    }
}

// without the trickle queue a down+up in the same frame is never seen pressed: release next poll
void ImguiBackendNcurses::_press_key(ImGuiIO & io, ImGuiKey key)
{
    io.AddKeyEvent(key, true);
    _pending_key_releases.push_back(key);
}

// a key pressed or released in this poll is only seen pressed again after a frame in the other state
bool ImguiBackendNcurses::_must_wait(ImGuiKey key) const
{
    return std::ranges::contains(_pending_key_releases, key) || std::ranges::contains(_released_keys, key);
}

void ImguiBackendNcurses::_press_chord(ImGuiIO & io, ImGuiKeyChord chord)
{
    if ((chord & ImGuiMod_Shift) != 0)
    {
        this->_press_key(io, ImGuiMod_Shift);
        this->_press_key(io, ImGuiKey_LeftShift);
    }
    if ((chord & ImGuiMod_Ctrl) != 0)
    {
        this->_press_key(io, ImGuiMod_Ctrl);
        this->_press_key(io, ImGuiKey_LeftCtrl);
    }
    // no LeftAlt: imgui toggles the menu layer when LeftAlt goes up after a press of its own
    if ((chord & ImGuiMod_Alt) != 0)
        this->_press_key(io, ImGuiMod_Alt);
    const ImGuiKey key = (ImGuiKey)(chord & ~ImGuiMod_Mask_);
    if (key != ImGuiKey_None)
        this->_press_key(io, key);
}

ImGuiKeyChord ImguiBackendNcurses::_key_chord(int key, bool function_key) const
{
    if (function_key)
    {
        if (const auto it = _modified_keys.find(key); it != _modified_keys.end())
            return it->second;
    }
    if (const ImGuiKey shifted = _ncurses_shifted_key_to_imgui(key); shifted != ImGuiKey_None)
        return ImGuiMod_Shift | shifted;
    if (const ImGuiKey plain = _ncurses_key_to_imgui(key); plain != ImGuiKey_None)
        return plain;
    if (const ImGuiKey ctrl_letter = _ctrl_letter_to_imgui(key); ctrl_letter != ImGuiKey_None)
        return ImGuiMod_Ctrl | ctrl_letter;
    return ImGuiKey_None;
}

bool ImguiBackendNcurses::_process_key_event(int key, bool function_key, ImGuiIO & io, bool alt)
{
    const bool printable = !function_key && (key >= 0x80 || (key >= 0x20 && key < 0x7F));
    const bool typed = printable && !alt;
    // imgui drops typed characters while Ctrl is down
    if (typed && this->_must_wait(ImGuiMod_Ctrl))
        return false;
    // a character past ASCII would collide with the KEY_* function key codes
    if (typed && key >= 0x80)
    {
        io.AddInputCharacter((unsigned int)key);
        return true;
    }

    const ImGuiKeyChord chord = (printable && alt) ? alt_character_chord(key)
                                                   : this->_key_chord(key, function_key) | (alt ? ImGuiMod_Alt : 0);
    const ImGuiKey pressed = (ImGuiKey)(chord & ~ImGuiMod_Mask_);
    if (pressed != ImGuiKey_None && this->_must_wait(pressed))
        return false;

    this->_press_chord(io, chord);
    if (typed)
        io.AddInputCharacter((unsigned int)key);
    return true;
}

// a terminal sends Alt+key as Escape then the key; Escape then [ is a sequence ncurses did not know
bool ImguiBackendNcurses::_read_alt_key(int & key, bool & function_key)
{
    wint_t next = 0;
    const int status = wget_wch(stdscr, &next);
    if (status == ERR)
        return false;
    if (status == KEY_CODE_YES && (next == KEY_MOUSE || next == KEY_RESIZE || next == paste_start_key))
    {
        ungetch((int)next);
        return false;
    }
    if (status != KEY_CODE_YES && next == '[')
    {
        wint_t final_byte = 0;
        while (wget_wch(stdscr, &final_byte) == OK && (final_byte < 0x40 || final_byte > 0x7E))
            continue;
        key = ERR;
        return true;
    }
    key = (int)next;
    function_key = status == KEY_CODE_YES;
    return true;
}

bool ImguiBackendNcurses::_process_input(int key, bool function_key, ImGuiIO & io)
{
    if (function_key && key == KEY_RESIZE)
        return true;
    if (function_key && key == KEY_MOUSE)
    {
        this->_process_mouse_event(io);
        return true;
    }
    if (function_key && (key == paste_start_key || key == paste_end_key))
    {
        _in_paste = key == paste_start_key;
        return true;
    }
    // pasted text goes in as characters: a pasted newline must not validate a field
    if (_in_paste)
    {
        if (!function_key)
            io.AddInputCharacter(key == '\r' ? '\n' : (unsigned int)key);
        return true;
    }

    const bool alt = !function_key && key == escape_key && this->_read_alt_key(key, function_key);
    if (key == ERR || this->_process_key_event(key, function_key, io, alt))
        return true;
    if (function_key)
        ungetch(key);
    else
        unget_wch((wchar_t)key);
    if (alt)
        unget_wch((wchar_t)escape_key);
    return false;
}

void ImguiBackendNcurses::poll()
{
    ImGuiIO & io = ImGui::GetIO();

    // cbreak keeps ISIG: Ctrl+C arrives as SIGINT, recorded by whoever installed sihd handlers
    if (sihd::sys::signal::should_stop())
        _should_close = true;

    // releases deferred from the previous poll: the button was down for a full frame
    for (int i = 0; i < (int)_buttons.size(); ++i)
    {
        MouseButton & button = _buttons[(size_t)i];
        button.pressed_this_poll = false;
        if (std::exchange(button.pending_release, false))
            this->_release_button(io, i);
    }
    for (auto it = _pending_key_releases.rbegin(); it != _pending_key_releases.rend(); ++it)
        io.AddKeyEvent(*it, false);
    _released_keys = std::move(_pending_key_releases);
    _pending_key_releases.clear();

    // idle: the first read sleeps until an input or the next idle frame
    wtimeout(stdscr, this->_idle_timeout_ms());
    while (true)
    {
        wint_t wch = 0;
        const int status = wget_wch(stdscr, &wch);
        wtimeout(stdscr, 0);
        if (status == ERR)
            break;
        _last_activity = std::chrono::steady_clock::now();
        if (!this->_process_input((int)wch, status == KEY_CODE_YES, io))
            break;
    }

    // mode-1003 emits dozens of motions per poll and only the last matters
    if (_mouse_seen && this->_virtual_pixels())
        io.AddMousePosEvent(((float)_mx + 0.5f) * _cell_size.x, ((float)_my + 0.5f) * _cell_size.y);
    else if (_mouse_seen)
        io.AddMousePosEvent((float)_mx + _grip_dx, (float)_my + _grip_dy);
    else
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

void ImguiBackendNcurses::new_frame()
{
    _frame_start = std::chrono::steady_clock::now();
    ImGuiIO & io = ImGui::GetIO();

    float delta = std::chrono::duration<float>(_frame_start - _last_time).count();
    io.DeltaTime = (delta > 0.0f) ? delta : (1.0f / 60.0f);
    _last_time = _frame_start;

    // do NOT clear() on KEY_RESIZE: it erases stdscr and blanks the screen
    int sx = 0, sy = 0;
    getmaxyx(stdscr, sy, sx);
    io.DisplaySize = {(float)sx * _cell_size.x, (float)sy * _cell_size.y};
}

void ImguiBackendNcurses::pre_render() {}

void ImguiBackendNcurses::post_render()
{
    // the ncurses renderer leaves stdscr untouched when the frame shows nothing new
    const bool changed = is_wintouched(stdscr);
    if (changed)
        _last_activity = std::chrono::steady_clock::now();
    if (_ime_visible)
        move(_ime_y, _ime_x + ime_caret_offset);
    curs_set(_ime_visible ? 1 : 0);
    // synchronized output (DEC 2026) shows a frame whole; a terminal without it ignores the mode
    if (changed)
    {
        std::fputs("\033[?2026h", stdout);
        std::fflush(stdout);
    }
    wrefresh(stdscr);
    if (changed)
    {
        std::fputs("\033[?2026l", stdout);
        std::fflush(stdout);
    }
    static constexpr long long target_us = 1000000LL / 60;
    const auto remaining = _frame_start + std::chrono::microseconds(target_us) - std::chrono::steady_clock::now();
    if (remaining > std::chrono::steady_clock::duration::zero())
        std::this_thread::sleep_for(remaining);
}

void ImguiBackendNcurses::_set_ime_data(ImGuiContext *, ImGuiViewport *, ImGuiPlatformImeData *data)
{
    auto *backend = static_cast<ImguiBackendNcurses *>(ImGui::GetPlatformIO().Platform_ImeUserData);
    backend->_ime_visible = data->WantVisible;
    backend->_ime_x = (int)std::lround(data->InputPos.x / backend->_cell_size.x);
    backend->_ime_y = (int)std::lround(data->InputPos.y / backend->_cell_size.y);
}

const char *ImguiBackendNcurses::_get_clipboard_text(ImGuiContext *)
{
    const auto *backend = static_cast<const ImguiBackendNcurses *>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
    return backend->_clipboard.c_str();
}

// sys::clipboard serves a copy for seconds before returning: the terminal keeps it instead, over ssh too
void ImguiBackendNcurses::_set_clipboard_text(ImGuiContext *, const char *text)
{
    auto *backend = static_cast<ImguiBackendNcurses *>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
    backend->_clipboard = text;
    if (const char *set_selection = terminfo_string("Ms"))
    {
        const std::string encoded = str::to_base64(backend->_clipboard);
        std::fputs(tiparm(set_selection, "c", encoded.c_str()), stdout);
        std::fflush(stdout);
    }
}

void ImguiBackendNcurses::shutdown()
{
    if (_is_init)
    {
        if (ImGui::GetCurrentContext() != nullptr)
        {
            ImGuiPlatformIO & platform_io = ImGui::GetPlatformIO();
            platform_io.Platform_SetImeDataFn = nullptr;
            platform_io.Platform_ImeUserData = nullptr;
            platform_io.Platform_GetClipboardTextFn = nullptr;
            platform_io.Platform_SetClipboardTextFn = nullptr;
            platform_io.Platform_ClipboardUserData = nullptr;
        }
        if (const char *paste_disable = terminfo_string("BD"))
            putp(paste_disable);
        putp("\033[?1003l");
        fflush(stdout);
        endwin();
        _is_init = false;
    }
}

void ImguiBackendNcurses::terminate() {}

} // namespace sihd::imgui
