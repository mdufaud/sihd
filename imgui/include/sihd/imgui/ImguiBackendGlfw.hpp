#ifndef __SIHD_IMGUI_IMGUIBACKENDGLFW_HPP__
#define __SIHD_IMGUI_IMGUIBACKENDGLFW_HPP__

#include <string>

#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <imgui_impl_glfw.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
# include <GLES2/gl2.h>
#endif

#include <sihd/imgui/IImguiBackend.hpp>

namespace sihd::imgui
{

class ImguiBackendGlfw: public sihd::imgui::IImguiBackend
{
    public:
        ImguiBackendGlfw();
        virtual ~ImguiBackendGlfw();

        bool init_window(const std::string & name, size_t width = 1280, size_t height = 800);
        bool init_backend_opengl();

        void new_frame();
        bool should_close();
        void poll();
        void pre_render();
        void post_render();
        void shutdown();
        void terminate();

        GLFWwindow *window() { return _glfw_window_ptr; };

    protected:

    private:
        bool _is_init;
        GLFWwindow *_glfw_window_ptr;
};

} // namespace sihd::imgui

#endif