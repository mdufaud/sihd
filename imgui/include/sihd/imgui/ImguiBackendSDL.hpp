#ifndef __SIHD_IMGUI_IMGUIBACKENDSDL_HPP__
#define __SIHD_IMGUI_IMGUIBACKENDSDL_HPP__

#include <string>

#include <SDL3/SDL.h>
#include <imgui_impl_sdl3.h>

#include <sihd/imgui/IImguiBackend.hpp>
#include <sihd/imgui/IImguiRenderer.hpp>
#include <sihd/util/platform.hpp>

#if !defined(__SIHD_WINDOWS__)

# if defined(IMGUI_IMPL_OPENGL_ES2)
#  include <SDL3/SDL_opengles2.h>
# else
#  include <SDL3/SDL_opengl.h>
# endif

#endif

namespace sihd::imgui
{

class ImguiBackendSDL: public sihd::imgui::IImguiBackend
{
    public:
        ImguiBackendSDL();
        virtual ~ImguiBackendSDL();

        void select_opengl2();
        void select_opengl3();
        void select_opengl32();
        void decide_opengl_version();
        // Set up an OpenGL context for rendering into an OpenGL window.
        void set_sdl_context_at_prerender(bool active);

        bool init_window(const std::string & name, size_t width = 1280, size_t height = 800);
        bool init_backend_opengl();
        bool init_backend_dx();
        void set_resize_renderer(IImguiRenderer *renderer);

        void new_frame();
        bool should_close();
        void poll();
        void pre_render();
        void post_render();
        void shutdown();
        void terminate();

        SDL_Window *window() { return _sdl_window_ptr; }
        SDL_GLContext context() { return _sdl_context_ptr; }
#if defined(__SIHD_WINDOWS__)
        void *windows_window();
#endif

    protected:
        virtual bool _sdl_init();

    private:
        bool _is_init;
        bool _close;
        bool _sdl_context_at_prerender;
        SDL_Window *_sdl_window_ptr;
        SDL_GLContext _sdl_context_ptr;
        IImguiRenderer *_imgui_renderer_ptr;
};

} // namespace sihd::imgui

#endif