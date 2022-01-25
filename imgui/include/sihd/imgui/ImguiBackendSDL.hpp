#ifndef __SIHD_IMGUI_IMGUIBACKENDSDL_HPP__
# define __SIHD_IMGUI_IMGUIBACKENDSDL_HPP__

# include <sihd/util/platform.hpp>
# include <sihd/imgui/IImguiBackend.hpp>
# include <sihd/imgui/IImguiRenderer.hpp>
# include <sihd/imgui/imgui_impl_sdl.h>
# include <SDL.h>
# if defined(IMGUI_IMPL_OPENGL_ES2)
#  include <SDL_opengles2.h>
# else
#  include <SDL_opengl.h>
# endif

# include <string>

namespace sihd::imgui
{

class ImguiBackendSDL: public sihd::imgui::IImguiBackend
{
    public:
        ImguiBackendSDL();
        virtual ~ImguiBackendSDL();

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
# if defined(__SIHD_WINDOWS__)
        HWND windows_window();
# endif

    protected:

    private:
        bool _is_init;
        bool _close;
        SDL_Window *_sdl_window_ptr;
        SDL_GLContext _sdl_context_ptr;
        IImguiRenderer *_imgui_renderer_ptr;
};

}

#endif