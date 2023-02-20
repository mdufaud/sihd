#ifndef __SIHD_IMGUI_IMGUIRENDERERDIRECTX_HPP__
#define __SIHD_IMGUI_IMGUIRENDERERDIRECTX_HPP__

#include <d3d11.h>
#include <sihd/imgui/IImguiRenderer.hpp>
#include <sihd/imgui/imgui_impl_dx11.h>
#include <tchar.h>

namespace sihd::imgui
{

class ImguiRendererDirectX: public sihd::imgui::IImguiRenderer
{
    public:
        ImguiRendererDirectX();
        virtual ~ImguiRendererDirectX();

        // Note: GLSL version is NOT the same as GL version. Leave this to NULL if unsure.
        bool init(HWND window);

        void new_frame();
        void render(ImDrawData *draw_data);
        void shutdown();

        void resize(LPARAM lParam);
        void resize();

        void set_clear_color(ImVec4 *clear_color);
        ImVec4 *get_clear_color();

    protected:
        virtual bool _create_dx_device(HWND window);
        virtual void _clean_dx();
        virtual void _clean_render_target();
        virtual void _create_render_target();

    private:
        bool _is_init;
        ID3D11Device *_dx_device_ptr;
        ID3D11DeviceContext *_dx_device_context_ptr;
        IDXGISwapChain *_dx_swap_chain_ptr;
        ID3D11RenderTargetView *_dx_main_render_target_view_ptr;
        ImVec4 *_clear_color_ptr;
};

} // namespace sihd::imgui

#endif