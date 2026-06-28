#ifndef __SIHD_IMGUI_IIMGUIRENDERER_HPP__
#define __SIHD_IMGUI_IIMGUIRENDERER_HPP__

#include <imgui.h>

namespace sihd::imgui
{

class IImguiRenderer
{
    public:
        virtual ~IImguiRenderer() = default;

        virtual void new_frame() = 0;
        virtual void render(ImDrawData *draw_data) = 0;
        virtual void shutdown() = 0;
        virtual void resize() = 0;

        virtual void set_clear_color(ImVec4 *clear_color) = 0;
        virtual ImVec4 *get_clear_color() = 0;
};

} // namespace sihd::imgui

#endif