#ifndef __SIHD_IMGUI_IMGUIRENDEREROPENGL_HPP__
#define __SIHD_IMGUI_IMGUIRENDEREROPENGL_HPP__

#if defined(__ANDROID__)
# include <GLES3/gl3.h>
#elif defined(IMGUI_IMPL_OPENGL_ES2)
# include <GLES2/gl2.h>
#else
# define GL_GLEXT_PROTOTYPES
# ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmodule-import-in-extern-c"
# endif
# include <GL/glcorearb.h>
# ifdef __clang__
#  pragma clang diagnostic pop
# endif
// Prevent GLAPI redefinition conflicts with SDL3/SDL_opengl.h
# undef GLAPI
#endif
#include <imgui_impl_opengl3.h>

#include <sihd/imgui/IImguiRenderer.hpp>

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