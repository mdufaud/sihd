#include <imgui.h>
#include <ncurses.h>
#include <sys/stat.h>

#include <cstdlib>
#include <thread>

#include <sihd/imgui/ImguiBackendNcurses.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::imgui
{

SIHD_LOGGER;

static ImGuiKey _ncurses_key_to_imgui(int key)
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

// Emit Shift + arrow combo (new imgui API)
static void _emit_shift_arrow(ImGuiIO & io, ImGuiKey arrow_key)
{
    io.AddKeyEvent(ImGuiKey_LeftShift, true);
    io.AddKeyEvent(arrow_key, true);
    io.AddKeyEvent(arrow_key, false);
    io.AddKeyEvent(ImGuiKey_LeftShift, false);
}

ImguiBackendNcurses::ImguiBackendNcurses():
    _is_init(false),
    _should_close(false),
    _mouse_seen(false),
    _ctrl_from_mouse(false),
    _pending_lrelease(false),
    _pending_rrelease(false),
    _mx(0),
    _my(0),
    _lbut(0),
    _rbut(0),
    _mstate(0)
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

    // When loaded via vcpkg's ncurses (which is a transitive dep of SDL3 etc.),
    // the compiled-in terminfo prefix points to the vcpkg install dir which is
    // not populated with terminfo data at runtime.
    // If no TERMINFO override is set by the user, append known system terminfo
    // directories to TERMINFO_DIRS so ncurses can fall back to them.
    if (getenv("TERMINFO") == nullptr)
    {
        static const char *candidates[] = {
            "/usr/share/terminfo",
            "/usr/lib/terminfo",
            "/etc/terminfo",
            nullptr,
        };
        std::string dirs;
        const char *existing = getenv("TERMINFO_DIRS");
        if (existing)
            dirs = existing;
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
            setenv("TERMINFO_DIRS", dirs.c_str(), /*overwrite=*/1);
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
    set_escdelay(25); // ESC responds in 25ms instead of 1s
    curs_set(0);
    mouseinterval(0); // disable click-detection delay: critical for drag
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
    // Enable all-motion mouse tracking (xterm mode 1003) so the terminal
    // sends position events on every cell movement, enabling window drag.
    printf("\033[?1003h\n");
    fflush(stdout);

    ImGuiIO & io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_ncurses";
    // Key repeat: imtui sets both to 50ms for snappy keyboard navigation
    io.KeyRepeatDelay = 0.050f;
    io.KeyRepeatRate = 0.050f;
    // Apply the whole input queue every frame instead of trickling events across
    // frames. Mode-1003 floods motion events; trickling would grow the deferred
    // queue unbounded under that flood, causing progressive hover/click lag.
    // Same-poll press+release is handled by the manual release-latch below.
    io.ConfigInputTrickleEventQueue = false;
    int sx = 0, sy = 0;
    getmaxyx(stdscr, sy, sx);
    io.DisplaySize = {(float)sx, (float)sy};

    _last_time = std::chrono::steady_clock::now();
    _frame_start = _last_time;
    _is_init = true;
    return true;
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

    // Left button. On press, mark it down and record that a press happened this
    // poll. On release: if a press landed in the same poll, defer the release one
    // poll (_pending_lrelease) so imgui sees the button down for a full NewFrame
    // and registers the click; otherwise release immediately.
    if (_mstate & BUTTON1_PRESSED)
    {
        pressed_l = true;
        if (!_lbut) { _lbut = 1; io.AddMouseButtonEvent(0, true); }
    }
    if (_mstate & BUTTON1_RELEASED)
    {
        if (pressed_l)
            _pending_lrelease = true;
        else if (_lbut) { _lbut = 0; io.AddMouseButtonEvent(0, false); }
    }
    if (_mstate & BUTTON3_PRESSED)
    {
        pressed_r = true;
        if (!_rbut) { _rbut = 1; io.AddMouseButtonEvent(1, true); }
    }
    if (_mstate & BUTTON3_RELEASED)
    {
        if (pressed_r)
            _pending_rrelease = true;
        else if (_rbut) { _rbut = 0; io.AddMouseButtonEvent(1, false); }
    }

    if (_mstate & BUTTON4_PRESSED)
        io.AddMouseWheelEvent(0.0f, 1.0f);
    if (_mstate & BUTTON5_PRESSED)
        io.AddMouseWheelEvent(0.0f, -1.0f);

    // Emit Ctrl only when the modifier bit actually transitions.
    const bool new_ctrl = (_mstate & BUTTON_CTRL) != 0;
    if (new_ctrl != _ctrl_from_mouse)
    {
        _ctrl_from_mouse = new_ctrl;
        io.AddKeyEvent(ImGuiKey_LeftCtrl, new_ctrl);
    }
}

bool ImguiBackendNcurses::_process_key_event(int key, ImGuiIO & io)
{
    if (key == 3)
        return true;

    if (key == 27)
    {
        io.AddKeyEvent(ImGuiKey_Escape, true);
        io.AddKeyEvent(ImGuiKey_Escape, false);
        return false;
    }

    // Shift+arrow keys
    if (key == KEY_SLEFT)  { _emit_shift_arrow(io, ImGuiKey_LeftArrow);  return false; }
    if (key == KEY_SRIGHT) { _emit_shift_arrow(io, ImGuiKey_RightArrow); return false; }
    if (key == KEY_SR)     { _emit_shift_arrow(io, ImGuiKey_UpArrow);    return false; }
    if (key == KEY_SF)     { _emit_shift_arrow(io, ImGuiKey_DownArrow);  return false; }
    if (key == KEY_SHOME)  { _emit_shift_arrow(io, ImGuiKey_Home);       return false; }
    if (key == KEY_SEND)   { _emit_shift_arrow(io, ImGuiKey_End);        return false; }

    ImGuiKey imgui_key = _ncurses_key_to_imgui(key);
    if (imgui_key != ImGuiKey_None)
    {
        io.AddKeyEvent(imgui_key, true);
        io.AddKeyEvent(imgui_key, false);
    }

    if (key >= 0x20 && key < 0x7F)
        io.AddInputCharacter((unsigned int)key);

    // Ctrl+A..Z combos (key==3 already handled above)
    if (key >= 1 && key <= 26 && key != 3 && imgui_key == ImGuiKey_None)
    {
        io.AddKeyEvent(ImGuiKey_LeftCtrl, true);
        io.AddKeyEvent((ImGuiKey)(ImGuiKey_A + key - 1), true);
        io.AddKeyEvent((ImGuiKey)(ImGuiKey_A + key - 1), false);
        io.AddKeyEvent(ImGuiKey_LeftCtrl, false);
    }
    return false;
}

void ImguiBackendNcurses::poll()
{
    ImGuiIO & io = ImGui::GetIO();

    // Flush releases deferred from the previous poll: the down state has now been
    // visible for a full frame, so the click was detected and we can release.
    if (_pending_lrelease) { _pending_lrelease = false; _lbut = 0; io.AddMouseButtonEvent(0, false); }
    if (_pending_rrelease) { _pending_rrelease = false; _rbut = 0; io.AddMouseButtonEvent(1, false); }

    // Track press transitions within THIS poll so a same-poll release gets latched.
    bool pressed_l = false, pressed_r = false;
    int n_mouse = 0, n_key = 0;

    while (true)
    {
        int key = wgetch(stdscr);

        if (key == ERR)
            break;

        if (key == KEY_RESIZE) { continue; }
        if (key == KEY_MOUSE)  { _process_mouse_event(io, pressed_l, pressed_r); ++n_mouse; continue; }

        ++n_key;
        if (_process_key_event(key, io))
            _should_close = true;
    }

    // Env-gated input diagnostics: SIHD_IMGUI_NCURSES_INPUT_DUMP=path appends one
    // line per poll (poll#, mouse-events drained, key-events, mx, my, lbut, rbut).
    // Lets us see if events accumulate (pty/ncurses side) or position lags.
    static FILE *dump = []() -> FILE * {
        const char *p = getenv("SIHD_IMGUI_NCURSES_INPUT_DUMP");
        return p ? fopen(p, "w") : nullptr;
    }();
    if (dump)
    {
        static unsigned long poll_idx = 0;
        fprintf(dump, "%lu %d %d %d %d %d %d\n", poll_idx++, n_mouse, n_key, _mx, _my, _lbut, _rbut);
        fflush(dump);
    }

    // Coalesce all motion into a single position event per poll (mode-1003 can emit
    // dozens per poll; trickle is off so only the last would matter anyway).
    if (_mouse_seen)
        io.AddMousePosEvent((float)_mx, (float)_my);
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

    // Update display size every frame (imtui: getmaxyx in NewFrame every time).
    // On KEY_RESIZE ncurses' own SIGWINCH handler already resized stdscr, so
    // getmaxyx reports the new size and LINES/COLS are up to date. The renderer
    // detects the size change and performs its own full redraw (werase + reset
    // shadow buffer); the backend must NOT call clear() here, as that erases
    // ncurses' stdscr buffer behind the renderer's differential update and
    // leaves the screen permanently blank after a resize.
    int sx = 0, sy = 0;
    getmaxyx(stdscr, sy, sx);
    io.DisplaySize = {(float)sx, (float)sy};
}

void ImguiBackendNcurses::pre_render() {}

void ImguiBackendNcurses::post_render()
{
    wrefresh(stdscr);
    static constexpr long long target_us = 1000000LL / 60;
    const auto remaining = _frame_start + std::chrono::microseconds(target_us) - std::chrono::steady_clock::now();
    if (remaining > std::chrono::steady_clock::duration::zero())
        std::this_thread::sleep_for(remaining);
}

void ImguiBackendNcurses::shutdown()
{
    if (_is_init)
    {
        printf("\033[?1003l\n"); // disable all-motion mouse tracking
        fflush(stdout);
        endwin();
        _is_init = false;
    }
}

void ImguiBackendNcurses::terminate() {}

} // namespace sihd::imgui
