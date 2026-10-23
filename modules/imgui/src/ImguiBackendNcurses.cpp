#include <imgui.h>
#include <imgui_internal.h>

// wide-char api: glibc g++ gets it from _GNU_SOURCE, musl needs it asked for
#define NCURSES_WIDECHAR 1
#include <ncursesw/ncurses.h>
// ncurses may define `bool` as a macro (musl cross): undef it before std/imgui headers
#undef bool
#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <vector>

#include <sihd/imgui/ImguiBackendNcurses.hpp>
#include <sihd/sys/env.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>

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

// imgui places the IME window one unit left of the text input caret
constexpr int ime_caret_offset = 1;
// ncurses merges a press and a release read together into a click, even with mouseinterval(0)
constexpr mmask_t button1_clicks = BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON1_TRIPLE_CLICKED;
constexpr mmask_t button3_clicks = BUTTON3_CLICKED | BUTTON3_DOUBLE_CLICKED | BUTTON3_TRIPLE_CLICKED;
// imgui's tooltip and double-click delays and app animations run on frames: full rate this long after an input or a change
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
    _pending_lrelease(false),
    _pending_rrelease(false),
    _ime_visible(false),
    _mx(0),
    _my(0),
    _lbut(0),
    _rbut(0),
    _grip_dx(0.0f),
    _grip_dy(0.0f),
    _ime_x(0),
    _ime_y(0),
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

    if (initscr() == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendNcurses: initscr() failed");
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
    fflush(stdout);

    ImGuiIO & io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_ncurses";
    io.KeyRepeatDelay = 0.050f;
    io.KeyRepeatRate = 0.050f;
    // apply the whole input queue every frame: mode-1003 motion floods would grow the deferred queue
    io.ConfigInputTrickleEventQueue = false;
    int sx = 0, sy = 0;
    getmaxyx(stdscr, sy, sx);
    io.DisplaySize = {(float)sx, (float)sy};

    ImGuiPlatformIO & platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_ImeUserData = this;
    platform_io.Platform_SetImeDataFn = &ImguiBackendNcurses::_set_ime_data;

    _last_time = std::chrono::steady_clock::now();
    _frame_start = _last_time;
    _last_activity = _last_time;
    _is_init = true;
    return true;
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

void ImguiBackendNcurses::_process_mouse_event(ImGuiIO & io, bool & pressed_l, bool & pressed_r)
{
    MEVENT ev;
    if (getmouse(&ev) != OK)
        return;

    _mx = ev.x;
    _my = ev.y;
    _mstate = ev.bstate;
    _mouse_seen = true;

    // a release after a same-poll press is deferred one poll so imgui registers the click
    if (_mstate & (BUTTON1_PRESSED | button1_clicks))
    {
        pressed_l = true;
        if (!_lbut)
        {
            _lbut = 1;
            const ImVec2 grip_offset = _resize_grip_offset(_mx, _my);
            _grip_dx = grip_offset.x;
            _grip_dy = grip_offset.y;
            io.AddMouseButtonEvent(0, true);
        }
    }

    if (_mstate & (BUTTON1_RELEASED | button1_clicks))
    {
        if (pressed_l)
            _pending_lrelease = true;
        else if (_lbut)
        {
            _lbut = 0;
            _grip_dx = 0.0f;
            _grip_dy = 0.0f;
            io.AddMouseButtonEvent(0, false);
        }
    }

    if (_mstate & (BUTTON3_PRESSED | button3_clicks))
    {
        pressed_r = true;
        if (!_rbut)
        {
            _rbut = 1;
            io.AddMouseButtonEvent(1, true);
        }
    }

    if (_mstate & (BUTTON3_RELEASED | button3_clicks))
    {
        if (pressed_r)
            _pending_rrelease = true;
        else if (_rbut)
        {
            _rbut = 0;
            io.AddMouseButtonEvent(1, false);
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

bool ImguiBackendNcurses::_process_key_event(int key, bool function_key, ImGuiIO & io)
{
    const bool printable = !function_key && (key >= 0x80 || (key >= 0x20 && key < 0x7F));
    // imgui drops typed characters while Ctrl is down
    if (printable && this->_must_wait(ImGuiMod_Ctrl))
        return false;
    // a character past ASCII would collide with the KEY_* function key codes
    if (!function_key && key >= 0x80)
    {
        io.AddInputCharacter((unsigned int)key);
        return true;
    }

    const ImGuiKey shifted = _ncurses_shifted_key_to_imgui(key);
    const ImGuiKey plain = _ncurses_key_to_imgui(key);
    const ImGuiKey ctrl_letter = _ctrl_letter_to_imgui(key);
    const ImGuiKey pressed = shifted != ImGuiKey_None ? shifted : plain != ImGuiKey_None ? plain : ctrl_letter;
    if (pressed != ImGuiKey_None && this->_must_wait(pressed))
        return false;

    if (shifted != ImGuiKey_None)
    {
        this->_press_key(io, ImGuiMod_Shift);
        this->_press_key(io, ImGuiKey_LeftShift);
    }
    else if (plain == ImGuiKey_None && ctrl_letter != ImGuiKey_None)
    {
        this->_press_key(io, ImGuiMod_Ctrl);
        this->_press_key(io, ImGuiKey_LeftCtrl);
    }
    if (pressed != ImGuiKey_None)
        this->_press_key(io, pressed);
    if (printable)
        io.AddInputCharacter((unsigned int)key);
    return true;
}

void ImguiBackendNcurses::poll()
{
    ImGuiIO & io = ImGui::GetIO();

    // cbreak keeps ISIG: Ctrl+C arrives as SIGINT, recorded by whoever installed sihd handlers
    if (sihd::sys::signal::should_stop())
        _should_close = true;

    // releases deferred from the previous poll: the button was down for a full frame
    if (_pending_lrelease)
    {
        _pending_lrelease = false;
        _lbut = 0;
        _grip_dx = 0.0f;
        _grip_dy = 0.0f;
        io.AddMouseButtonEvent(0, false);
    }
    if (_pending_rrelease)
    {
        _pending_rrelease = false;
        _rbut = 0;
        io.AddMouseButtonEvent(1, false);
    }
    for (auto it = _pending_key_releases.rbegin(); it != _pending_key_releases.rend(); ++it)
        io.AddKeyEvent(*it, false);
    _released_keys = std::move(_pending_key_releases);
    _pending_key_releases.clear();

    bool pressed_l = false, pressed_r = false;

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

        const int key = (int)wch;
        const bool function_key = status == KEY_CODE_YES;
        if (function_key && key == KEY_RESIZE)
            continue;
        if (function_key && key == KEY_MOUSE)
        {
            _process_mouse_event(io, pressed_l, pressed_r);
            continue;
        }
        if (!this->_process_key_event(key, function_key, io))
        {
            if (function_key)
                ungetch(key);
            else
                unget_wch((wchar_t)wch);
            break;
        }
    }

    // mode-1003 emits dozens of motions per poll and only the last matters
    if (_mouse_seen)
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
    io.DisplaySize = {(float)sx, (float)sy};
}

void ImguiBackendNcurses::pre_render() {}

void ImguiBackendNcurses::post_render()
{
    // the ncurses renderer leaves stdscr untouched when the frame shows nothing new
    if (is_wintouched(stdscr))
        _last_activity = std::chrono::steady_clock::now();
    if (_ime_visible)
        move(_ime_y, _ime_x + ime_caret_offset);
    curs_set(_ime_visible ? 1 : 0);
    wrefresh(stdscr);
    static constexpr long long target_us = 1000000LL / 60;
    const auto remaining = _frame_start + std::chrono::microseconds(target_us) - std::chrono::steady_clock::now();
    if (remaining > std::chrono::steady_clock::duration::zero())
        std::this_thread::sleep_for(remaining);
}

void ImguiBackendNcurses::_set_ime_data(ImGuiContext *, ImGuiViewport *, ImGuiPlatformImeData *data)
{
    auto *backend = static_cast<ImguiBackendNcurses *>(ImGui::GetPlatformIO().Platform_ImeUserData);
    backend->_ime_visible = data->WantVisible;
    backend->_ime_x = (int)std::lround(data->InputPos.x);
    backend->_ime_y = (int)std::lround(data->InputPos.y);
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
        }
        putp("\033[?1003l");
        fflush(stdout);
        endwin();
        _is_init = false;
    }
}

void ImguiBackendNcurses::terminate() {}

} // namespace sihd::imgui
