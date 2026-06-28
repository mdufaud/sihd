#ifndef __SIHD_IMGUI_IMGUIBACKENDWIN32_HPP__
#define __SIHD_IMGUI_IMGUIBACKENDWIN32_HPP__

#include <tchar.h>

#include <string>

#include <imgui_impl_win32.h>

#include <sihd/imgui/IImguiBackend.hpp>
#include <sihd/imgui/IImguiRenderer.hpp>
#include <sihd/imgui/ImguiRendererDirectX.hpp>

namespace sihd::imgui
{

class ImguiBackendWin32: public sihd::imgui::IImguiBackend
{
    public:
        ImguiBackendWin32();
        virtual ~ImguiBackendWin32();

        bool init_window(const std::string & name, size_t width = 1280, size_t height = 800);
        bool init_backend();
        void set_resize_renderer(IImguiRenderer *renderer);

        void new_frame();
        bool should_close();
        void poll();
        void pre_render();
        void post_render();
        void shutdown();
        void terminate();

        LRESULT handle_win32_msg_handler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        HWND window() { return _window; }

    protected:

    private:
        bool _close;
        bool _is_init;

        WNDCLASSEX _win_cls;
        HWND _window;

        ImguiRendererDirectX *_imgui_renderer_dx_ptr;
        IImguiRenderer *_imgui_renderer_ptr;
};

} // namespace sihd::imgui

#endif