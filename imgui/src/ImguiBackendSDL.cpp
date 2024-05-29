#include <sihd/imgui/ImguiBackendSDL.hpp>
#include <sihd/util/Logger.hpp>

#if defined(__SIHD_WINDOWS__)
# include <SDL_syswm.h>
#endif

namespace sihd::imgui
{

SIHD_LOGGER;

ImguiBackendSDL::ImguiBackendSDL(): _sdl_window_ptr(nullptr), _sdl_context_ptr(nullptr), _imgui_renderer_ptr(nullptr)
{
    _is_init = false;
    _close = false;
    _sdl_context_at_prerender = false;
    this->_sdl_init();
}

ImguiBackendSDL::~ImguiBackendSDL()
{
    this->shutdown();
    this->terminate();
}

bool ImguiBackendSDL::_sdl_init()
{
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
    {
        // Setup SDL
        // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows
        // systems, depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of
        // SDL is recommended!)
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
        {
            SIHD_LOG(error, "ImguiBackendSDL: " << SDL_GetError());
            return false;
        }
    }
    return true;
}

void ImguiBackendSDL::select_opengl2()
{
    // GL ES 2.0 + GLSL 100
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
}

void ImguiBackendSDL::select_opengl3()
{
    // GL 3.0 + GLSL 130
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
}

void ImguiBackendSDL::select_opengl32()
{
    // GL 3.2 Core + GLSL 150
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
}

void ImguiBackendSDL::decide_opengl_version()
{
#if defined(IMGUI_IMPL_OPENGL_ES2)
    this->select_opengl2();
#elif defined(__APPLE__)
    this->select_opengl32();
#else
    this->select_opengl3();
#endif
}

bool ImguiBackendSDL::init_window(const std::string & name, size_t width, size_t height)
{
    if (_sdl_window_ptr != nullptr)
    {
        SIHD_LOG(warning, "ImguiBackendSDL: already initialized");
        return true;
    }
    if (this->_sdl_init() == false)
        return false;
    _close = false;
    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags
        = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    _sdl_window_ptr
        = SDL_CreateWindow(name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
    if (_sdl_window_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendSDL: " << SDL_GetError());
        return false;
    }
    return true;
}

bool ImguiBackendSDL::init_backend_opengl()
{
    if (_sdl_window_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendSDL: window not initialized");
        return false;
    }
    _sdl_context_ptr = SDL_GL_CreateContext(_sdl_window_ptr);
    if (_sdl_context_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendSDL: " << SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(_sdl_window_ptr, _sdl_context_ptr);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    _is_init = ImGui_ImplSDL2_InitForOpenGL(_sdl_window_ptr, _sdl_context_ptr);
    return _is_init;
}

bool ImguiBackendSDL::init_backend_dx()
{
    if (_sdl_window_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendSDL: window not initialized");
        return false;
    }
    _is_init = ImGui_ImplSDL2_InitForD3D(_sdl_window_ptr);
    return _is_init;
}

void ImguiBackendSDL::set_resize_renderer(IImguiRenderer *renderer)
{
    _imgui_renderer_ptr = renderer;
}

void ImguiBackendSDL::set_sdl_context_at_prerender(bool active)
{
    _sdl_context_at_prerender = active;
}

void ImguiBackendSDL::new_frame()
{
    ImGui_ImplSDL2_NewFrame();
}

void ImguiBackendSDL::poll()
{
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your
    // inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two
    // flags.
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
            _close = true;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE
            && event.window.windowID == SDL_GetWindowID(_sdl_window_ptr))
            _close = true;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED
            && event.window.windowID == SDL_GetWindowID(_sdl_window_ptr))
        {
            if (_imgui_renderer_ptr != nullptr)
                _imgui_renderer_ptr->resize();
        }
    }
}

bool ImguiBackendSDL::should_close()
{
    return _close;
}

void ImguiBackendSDL::pre_render()
{
    if (_sdl_context_at_prerender)
        SDL_GL_MakeCurrent(_sdl_window_ptr, _sdl_context_ptr);
}

void ImguiBackendSDL::post_render()
{
    SDL_GL_SwapWindow(_sdl_window_ptr);
}

void ImguiBackendSDL::shutdown()
{
    if (_is_init)
    {
        ImGui_ImplSDL2_Shutdown();
        _is_init = false;
    }
}

void ImguiBackendSDL::terminate()
{
    if (_sdl_window_ptr != nullptr)
    {
        SDL_GL_DeleteContext(_sdl_context_ptr);
        _sdl_context_ptr = nullptr;
        SDL_DestroyWindow(_sdl_window_ptr);
        _sdl_window_ptr = nullptr;
        SDL_Quit();
    }
}

#if defined(__SIHD_WINDOWS__)
HWND ImguiBackendSDL::windows_window()
{
    if (_sdl_window_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendSDL: window not initialized");
        return nullptr;
    }
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(_sdl_window_ptr, &wmInfo);
    return (HWND)wmInfo.info.win.window;
}
#endif

} // namespace sihd::imgui