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
- `ImguiBackendNcurses`: terminal backend reading keyboard and mouse input through ncurses. `init`.

### Renderer interface

- `IImguiRenderer`: common contract: `new_frame`, `render(ImDrawData *)`, `shutdown`, `resize`, `set_clear_color(ImVec4 *)` (stores the pointer), `get_clear_color`.
- `ImguiRendererOpenGL`: OpenGL renderer (GLES3 on Android). `init(glsl_version = nullptr)`.
- `ImguiRendererDirectX`: DirectX 11 renderer. `init(HWND)`, `resize()`, `resize(LPARAM)`.
- `ImguiRendererNcurses`: draws ImGui geometry into a terminal cell grid and flushes it to ncurses with differential updates. `init`. Headless test helpers: `init_headless(w, h)`, `rasterize_headless(ImDrawData *)`, `get_cell(x, y)`, `clear_screen`. Static colour helpers: `rgb_to_ansi256`, `col_to_ansi256`.

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

## Scripting

Not exposed to Lua or Python.
