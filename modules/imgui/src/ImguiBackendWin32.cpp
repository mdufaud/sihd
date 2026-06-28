#include <sihd/imgui/ImguiBackendWin32.hpp>
#include <sihd/util/Logger.hpp>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    sihd::imgui::ImguiBackendWin32 *backend_ptr
        = (sihd::imgui::ImguiBackendWin32 *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (backend_ptr != nullptr)
        return backend_ptr->handle_win32_msg_handler(hWnd, msg, wParam, lParam);
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

namespace sihd::imgui
{

SIHD_LOGGER;

ImguiBackendWin32::ImguiBackendWin32(): _is_init(false), _window(nullptr), _imgui_renderer_ptr(nullptr) {}

ImguiBackendWin32::~ImguiBackendWin32()
{
    this->shutdown();
    this->terminate();
}

bool ImguiBackendWin32::init_window(const std::string & name, size_t width, size_t height)
{
    if (_window != nullptr)
    {
        SIHD_LOG(warning, "ImguiBackendWin32: already initialized");
        return true;
    }
    _close = false;
    _is_init = false;
    _win_cls.cbSize = sizeof(WNDCLASSEX);
    _win_cls.style = CS_CLASSDC;
    _win_cls.lpfnWndProc = WndProc;
    _win_cls.cbClsExtra = 0L;
    _win_cls.cbWndExtra = 0L;
    _win_cls.hInstance = GetModuleHandle(NULL);
    _win_cls.hIcon = NULL;
    _win_cls.hCursor = NULL;
    _win_cls.hbrBackground = NULL;
    _win_cls.lpszMenuName = NULL;
    _win_cls.lpszClassName = _T("sihd-imgui-backend-win32");
    _win_cls.hIconSm = NULL;
    ::RegisterClassEx(&_win_cls);
    _window = ::CreateWindow(_win_cls.lpszClassName,
                             name.c_str(),
                             WS_OVERLAPPEDWINDOW,
                             // x - y - width - height
                             100,
                             100,
                             width,
                             height,
                             NULL,
                             NULL,
                             _win_cls.hInstance,
                             this);
    if (_window != nullptr)
    {
        ::SetWindowLongPtr(_window, GWLP_USERDATA, (LONG_PTR)this);
        ::SetWindowPos(_window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }
    else
        SIHD_LOG(error, "ImguiBackendWin32: CreateWindow failed");
    return _window != nullptr;
}

bool ImguiBackendWin32::init_backend()
{
    if (_window == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendWin32: window not initialized");
        return false;
    }
    ::ShowWindow(_window, SW_SHOWDEFAULT);
    ::UpdateWindow(_window);
    _is_init = ImGui_ImplWin32_Init(_window);
    if (!_is_init)
        SIHD_LOG(error, "ImguiBackendWin32: ImGui_ImplWin32_Init failed");
    return _is_init;
}

void ImguiBackendWin32::set_resize_renderer(IImguiRenderer *renderer)
{
    _imgui_renderer_ptr = renderer;
}

LRESULT ImguiBackendWin32::handle_win32_msg_handler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg)
    {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED && _imgui_renderer_ptr != nullptr)
                _imgui_renderer_ptr->resize();
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void ImguiBackendWin32::new_frame()
{
    ImGui_ImplWin32_NewFrame();
}

void ImguiBackendWin32::poll()
{
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            _close = true;
    }
}

bool ImguiBackendWin32::should_close()
{
    return _close;
}

void ImguiBackendWin32::pre_render() {}

void ImguiBackendWin32::post_render() {}

void ImguiBackendWin32::shutdown()
{
    if (_is_init)
    {
        ImGui_ImplWin32_Shutdown();
        _is_init = false;
    }
}

void ImguiBackendWin32::terminate()
{
    if (_window != nullptr)
    {
        ::DestroyWindow(_window);
        _window = nullptr;
        ::UnregisterClass(_win_cls.lpszClassName, _win_cls.hInstance);
    }
}

} // namespace sihd::imgui