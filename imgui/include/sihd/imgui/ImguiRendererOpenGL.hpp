#ifndef __SIHD_IMGUI_IMGUIRENDEREROPENGL_HPP__
#define __SIHD_IMGUI_IMGUIRENDEREROPENGL_HPP__

#include <GL/gl.h>
#include <sihd/imgui/IImguiRenderer.hpp>
#include <sihd/imgui/imgui_impl_opengl3.h>

namespace sihd::imgui
{

class ImguiRendererOpenGL: public sihd::imgui::IImguiRenderer
{
    public:
        ImguiRendererOpenGL();
        virtual ~ImguiRendererOpenGL();

        // Note: GLSL version is NOT the same as GL version. Leave this to NULL if unsure.
        bool init(const char *glsl_version = nullptr);

        void new_frame();
        void render(ImDrawData *draw_data);
        void shutdown();
        void resize();

        void set_clear_color(ImVec4 *clear_color);
        ImVec4 *get_clear_color();

    protected:

    private:
        bool _is_init;
        ImVec4 *_clear_color_ptr;
};

} // namespace sihd::imgui

#endif