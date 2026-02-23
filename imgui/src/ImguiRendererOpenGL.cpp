#include <sihd/imgui/ImguiRendererOpenGL.hpp>
// Include imgui's GL loader to redirect glViewport/glClear/glClearColor
// to dynamically loaded function pointers (avoids link-time dependency on libGL)
#include <imgui_impl_opengl3_loader.h>
#include <sihd/util/Logger.hpp>

namespace sihd::imgui
{

SIHD_LOGGER;

ImguiRendererOpenGL::ImguiRendererOpenGL(): _is_init(false)
{
    _clear_color_ptr = nullptr;
}

ImguiRendererOpenGL::~ImguiRendererOpenGL()
{
    this->shutdown();
}

void ImguiRendererOpenGL::set_clear_color(ImVec4 *clear_color)
{
    _clear_color_ptr = clear_color;
}

ImVec4 *ImguiRendererOpenGL::get_clear_color()
{
    return _clear_color_ptr;
}

bool ImguiRendererOpenGL::init(const char *glsl_version)
{
    if (_is_init)
    {
        SIHD_LOG(warning, "ImguiRendererOpenGL: already initialized");
        return true;
    }
    if (_clear_color_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiRendererOpenGL: cannot init before a clear color vector is set");
        return false;
    }
    _is_init = ImGui_ImplOpenGL3_Init(glsl_version);
    return _is_init;
}

void ImguiRendererOpenGL::new_frame()
{
    ImGui_ImplOpenGL3_NewFrame();
}

void ImguiRendererOpenGL::resize()
{
    // nothing to do
}

void ImguiRendererOpenGL::render(ImDrawData *draw_data)
{
    ImGuiIO & io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(_clear_color_ptr->x * _clear_color_ptr->w,
                 _clear_color_ptr->y * _clear_color_ptr->w,
                 _clear_color_ptr->z * _clear_color_ptr->w,
                 _clear_color_ptr->w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
}

void ImguiRendererOpenGL::shutdown()
{
    if (_is_init)
        ImGui_ImplOpenGL3_Shutdown();
    _is_init = false;
}

} // namespace sihd::imgui