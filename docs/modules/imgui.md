# imgui

Dear ImGui application loop. `ImguiRunner` drives one platform backend (window and input) and one renderer (draw calls) and calls a user-supplied function to build each frame. The backend and renderer available depend on the target platform.

| | |
|---|---|
| Depends on | `util`, `sys` |
| External libraries | imgui, opengl (SDL3, GLFW and ncursesw for their respective backends) |
| Platforms | Linux, Windows, Android, Web (emscripten) |
| Build | `make m=imgui` |
| Header | `#include <sihd/imgui/ImguiRunner.hpp>` (umbrella header: `#include <sihd/imgui.hpp>`) |

## Capabilities

### Loop

- `ImguiRunner`: owns the frame loop. `set_backend`, `set_renderer`, `set_build_frame(std::function<bool()>)` configure it, `init_imgui` initializes ImGui, `run` starts the loop, `stop` ends it, `is_running` reports the state. Protected virtual hooks (`_new_frame`, `_build_frame`, `_render`, `_loop`, `_shutdown`) can be overridden by a subclass. On emscripten, `run` hands the loop to the browser main loop instead of blocking.

### Backend interface

- `IImguiBackend`: common contract of every backend: `new_frame`, `should_close`, `poll`, `pre_render`, `post_render`, `shutdown`, `terminate`.
- `ImguiBackendGlfw`: GLFW window with OpenGL context. `init_window(name, width, height)`, `init_backend_opengl`, `window`.
- `ImguiBackendSDL`: SDL3 window. `init_window(name, width, height)`, `decide_opengl_version`, `select_opengl2`, `select_opengl3`, `select_opengl32`, `init_backend_opengl`, `init_backend_dx` (Windows), `set_resize_renderer`, `window`, `context`.
- `ImguiBackendWin32`: native Win32 window, meant to be paired with `ImguiRendererDirectX`. `init_window`, `init_backend`, `set_resize_renderer`, `handle_win32_msg_handler`, `window`.
- `ImguiBackendAndroid`: NativeActivity with EGL. `set_app`, `init_egl`, `init_backend`, `window`.
- `ImguiBackendNcurses`: terminal backend reading keyboard and mouse input through ncurses. `init`. Frames run at up to 60 per second; once the screen stayed still and no input came for a second they drop to the idle frequency (`set_idle_frequency(hz)`, 10 by default, false when not positive), and the next input wakes the loop at once. A screen that keeps changing (animated plots, live values, an FPS counter) keeps the full rate; a change on a still screen shows within one idle frame. `ImguiRendererNcurses` leaves stdscr untouched when a frame matches the last one, which is how the backend tells. Wide-character input (UTF-8 typing), Shift+Tab, Ctrl+letter, and arrows, Home, End, Page Up/Down, Insert and Delete with any mix of Shift, Alt and Ctrl the terminfo describes (`kLFT5` is Ctrl+Left), all with the matching imgui modifiers; Alt+letter and Alt+digit (Escape then the key) are shortcuts that type nothing and never toggle the menu layer, and an escape sequence ncurses does not know is dropped instead of typed. SGR mouse with any-motion reporting, left, right and middle buttons. With a terminfo that has `BE`, `PS` and `PE` (xterm-like terminals), bracketed paste is on: pasted text is typed as characters, so a pasted newline neither validates a field nor presses Enter. Copied or cut text is kept for imgui's own paste and sent to the terminal clipboard through OSC 52 when the terminfo has `Ms`, which also works over ssh; `sys::clipboard` is not used, its `set` serves the copy for seconds before returning. Text from other programs comes in through the terminal's paste. Non-character keys are released one poll after their press so imgui sees them pressed; a key repeated within one read (autorepeat, a burst over ssh) waits for the next poll, and so does text typed while Ctrl is held. A press and release read together, which ncurses reports as a click, still reach imgui as a press then a release. Ctrl+C ends the loop when the program records signals through `sihd::sys::signal` (as `sihd::sys::App` does). The text input caret is shown as the terminal cursor. Each frame that changes is wrapped in synchronized output (DEC mode 2026), so a terminal that knows it never shows half a frame; others ignore the mode. When `COLORTERM` is `truecolor` or `24bit` and `TERM` names a `-256color` terminfo, `init` opens the matching `-direct` terminfo (`xterm-256color` becomes `xterm-direct`) for 24-bit colours, and keeps `TERM` when that terminfo is not installed.

### Renderer interface

- `IImguiRenderer`: common contract: `new_frame`, `render(ImDrawData *)`, `shutdown`, `resize`, `set_clear_color(ImVec4 *)` (stores the pointer), `get_clear_color`.
- `ImguiRendererOpenGL`: OpenGL renderer (GLES3 on Android). `init(glsl_version = nullptr)`.
- `ImguiRendererDirectX`: DirectX 11 renderer. `init(HWND)`, `resize()`, `resize(LPARAM)`.
- `ImguiRendererNcurses`: draws ImGui geometry into a terminal cell grid and flushes it to ncurses with differential updates. `init`. Output is Unicode when the locale is multibyte (the program calls `setlocale(LC_ALL, "")` before `init`), ASCII otherwise; `set_unicode(bool)` forces either. It installs a font loader giving every character one cell (two for wide characters), so any Unicode text renders without a font file. Each style colour is nudged to a distinct RGB value so the renderer knows which style colour drew a shape: window, child and popup borders become box drawing (`┌─┐`), table borders vertical lines, check marks `✓`, arrows `▶▼`, bullets `•`, close buttons `×`, resize grips `◢`, plot lines braille dots, histograms and progress bars eighth blocks, scrollbars `░` with a `█` grab, slider grabs and tabs their background colour (the selected tab its own), the navigation cursor and selected text reverse video, modal backgrounds dim, `ImGui::Image` (the font atlas included) a `▒` block. `SeparatorText` puts its line on the label row, menu bars of windows without a title bar (`BeginMainMenuBar` included) keep their row, a mixed checkbox shows `-`, control characters take no cell (a tab advances four), and a glyph imgui clips down to nothing at a frame edge is dropped. Other shapes are filled, with the gradient of `AddRectFilledMultiColor` kept per cell. Translucent colours are blended over the cell below; colours are reduced to 256, 16 or 8 entries depending on the terminal, sent as 24-bit with a direct-colour terminfo (`TERM=xterm-direct`, or `COLORTERM` as above), or left out on a terminal without colours. `init` turns on `io.ConfigDebugDrawListDefaultsToStrokeLegacy`, the only stock way to get slider grabs and one-row tab backgrounds drawn: `AddLine` calls of the application are then offset by half a unit, as before imgui 1.93. The style keeps a window title row on screen (`DisplayWindowPadding` 4x1) and opens tooltips next to the mouse (`MouseCursorScale` 0.1). Only the cells inside the clip rect and the screen are visited, so huge or off-screen shapes cost nothing. Headless test helpers: `init_headless(w, h)` (needs a current ImGui context, applies the same font and style as `init`), `rasterize_headless(ImDrawData *)`, `get_cell(x, y)`, `clear_screen`. Static helpers: `rgb_to_ansi256`, `classify_arrow`, `is_cross_pattern`, `compute_clip_rect`.
- Ncurses limitations (stock imgui at one cell per frame row): radio buttons draw nothing (only their label shows), font sizes other than the base size keep one cell per character but stretch the line height, table rows have no horizontal borders, a scrolling table with outer horizontal borders loses its first and last visible row (imgui insets its clip rect by one border unit), auto-resized windows (tooltips, popups) are at least 4 rows tall. Sizes and offsets the application gives in pixels are taken as cells: the official demo's `Button("Button 1", ImVec2(80, 80))` is 80 by 80 cells, its default window position (60, 60) is far down a terminal, and outlines drawn one pixel around an item land on the row next to it.

### Backend and renderer per platform

- Linux: backends `ImguiBackendGlfw`, `ImguiBackendSDL`, `ImguiBackendNcurses`; renderers `ImguiRendererOpenGL`, `ImguiRendererNcurses`.
- Windows: backends `ImguiBackendGlfw`, `ImguiBackendSDL`, `ImguiBackendWin32`; renderers `ImguiRendererOpenGL`, `ImguiRendererDirectX`.
- Android: backend `ImguiBackendAndroid`; renderer `ImguiRendererOpenGL` (OpenGL ES 3).
- Web (emscripten): backend `ImguiBackendSDL`; renderer `ImguiRendererOpenGL`. GLFW is not built.

Ncurses classes are built on Linux only. `imgui.hpp` includes the ncurses headers unconditionally, so linking them on other platforms fails.

## Example

```cpp
#include <sihd/imgui/ImguiBackendSDL.hpp>
#include <sihd/imgui/ImguiRendererOpenGL.hpp>
#include <sihd/imgui/ImguiRunner.hpp>

using namespace sihd::imgui;

int main()
{
    ImguiRunner imgui("imgui-runner");
    imgui.init_imgui();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImguiRendererOpenGL opengl_renderer;
    opengl_renderer.set_clear_color(&clear_color);

    ImguiBackendSDL sdl_backend;
    sdl_backend.decide_opengl_version();
    sdl_backend.init_window("OpenGL3 SDL test");
    sdl_backend.init_backend_opengl();
    opengl_renderer.init();

    imgui.set_backend(&sdl_backend);
    imgui.set_renderer(&opengl_renderer);
    imgui.set_build_frame([&]() -> bool {
        ImGui::Begin("Hello");
        ImGui::ColorEdit3("clear color", (float *)&clear_color);
        ImGui::End();
        return true;
    });
    imgui.run();
    return 0;
}
```

Adapted from `modules/imgui/test/TestOpenGL3_SDL.cpp`.

## Testing the ncurses backend

Headless screen tests (`TestNcursesScene`, `TestNcursesWidgets`, `TestNcursesLayout`, `TestNcursesDraw`, `TestNcursesOverlays`, `TestNcursesShapes`, `TestNcursesText`, on the `test/helper/NcursesScene` base) run real imgui frames through the production font and style and compare the cell grid with screen literals; they are part of `make itest m=imgui`.

Terminal tests drive `sihd_imgui_ncurses_scenes` (built with `test=1`) in a pty and read the screen through the pyte VT emulator, so they cover the ncurses input decoding, the idle frame rate and the output, on `xterm-256color` (with and without `COLORTERM`), `screen-256color`, `linux`, `xterm-direct` and `vt100` and in an ASCII locale. `COLORTERM` from the calling shell is not passed to the scene. They need the `pytest` and `pyte` Python packages; `scripts/test.sh` runs them after the native clang build when `python3` has both, and skips them otherwise:

```bash
python3 -m pytest -q -p no:cacheprovider modules/imgui/test/tui
```

`SIHD_IMGUI_SCENES_BIN` overrides the binary path (default `build/last/bin/sihd_imgui_ncurses_scenes`). To look at the scene as a terminal shows it:

```bash
python3 modules/imgui/test/tui/tui_snapshot.py --size 100x30 --term linux --ansi
```

## Scripting

Not exposed to Lua or Python.
