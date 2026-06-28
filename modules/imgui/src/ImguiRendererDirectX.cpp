#include <sihd/imgui/ImguiRendererDirectX.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::imgui
{

SIHD_LOGGER;

ImguiRendererDirectX::ImguiRendererDirectX():
    _is_init(false),
    _dx_device_ptr(nullptr),
    _dx_device_context_ptr(nullptr),
    _dx_swap_chain_ptr(nullptr),
    _dx_main_render_target_view_ptr(nullptr)
{
    _clear_color_ptr = nullptr;
}

ImguiRendererDirectX::~ImguiRendererDirectX()
{
    this->shutdown();
}

void ImguiRendererDirectX::set_clear_color(ImVec4 *clear_color)
{
    _clear_color_ptr = clear_color;
}

ImVec4 *ImguiRendererDirectX::get_clear_color()
{
    return _clear_color_ptr;
}

bool ImguiRendererDirectX::init(HWND window)
{
    if (_is_init)
    {
        SIHD_LOG(warning, "ImguiRendererDirectX: already initialized");
        return true;
    }
    if (_clear_color_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiRendererDirectX: cannot init before a clear color vector is set");
        return false;
    }
    bool ret = this->_create_dx_device(window);
    if (ret)
    {
        ret = ImGui_ImplDX11_Init(_dx_device_ptr, _dx_device_context_ptr);
        if (!ret)
            SIHD_LOG(error, "ImguiRendererDirectX: ImGui_ImplDX11_Init failed");
        _is_init = ret;
    }
    else
        SIHD_LOG(error, "ImguiRendererDirectX: D3D11CreateDeviceAndSwapChain failed");
    return ret;
}

void ImguiRendererDirectX::resize(LPARAM lParam)
{
    if (_dx_device_ptr != nullptr)
    {
        this->_clean_render_target();
        _dx_swap_chain_ptr->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
        this->_create_render_target();
    }
}

void ImguiRendererDirectX::resize()
{
    if (_dx_device_ptr != nullptr)
    {
        this->_clean_render_target();
        _dx_swap_chain_ptr->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        this->_create_render_target();
    }
}

void ImguiRendererDirectX::new_frame()
{
    ImGui_ImplDX11_NewFrame();
}

void ImguiRendererDirectX::render(ImDrawData *draw_data)
{
    const float clear_color_with_alpha[4] = {_clear_color_ptr->x * _clear_color_ptr->w,
                                             _clear_color_ptr->y * _clear_color_ptr->w,
                                             _clear_color_ptr->z * _clear_color_ptr->w,
                                             _clear_color_ptr->w};
    _dx_device_context_ptr->OMSetRenderTargets(1, &_dx_main_render_target_view_ptr, nullptr);
    _dx_device_context_ptr->ClearRenderTargetView(_dx_main_render_target_view_ptr, clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(draw_data);
    _dx_swap_chain_ptr->Present(1, 0); // Present with vsync
    //_dx_swap_chain_ptr->Present(0, 0); // Present without vsync
}

void ImguiRendererDirectX::shutdown()
{
    if (_is_init)
    {
        ImGui_ImplDX11_Shutdown();
        _is_init = false;
    }
    this->_clean_dx();
}

bool ImguiRendererDirectX::_create_dx_device(HWND window)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = window;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    if (D3D11CreateDeviceAndSwapChain(nullptr,
                                      D3D_DRIVER_TYPE_HARDWARE,
                                      nullptr,
                                      createDeviceFlags,
                                      featureLevelArray,
                                      2,
                                      D3D11_SDK_VERSION,
                                      &sd,
                                      &_dx_swap_chain_ptr,
                                      &_dx_device_ptr,
                                      &featureLevel,
                                      &_dx_device_context_ptr)
        != S_OK)
        return false;
    this->_create_render_target();
    return true;
}

void ImguiRendererDirectX::_create_render_target()
{
    ID3D11Texture2D *pBackBuffer;
    _dx_swap_chain_ptr->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    _dx_device_ptr->CreateRenderTargetView(pBackBuffer, nullptr, &_dx_main_render_target_view_ptr);
    pBackBuffer->Release();
}

void ImguiRendererDirectX::_clean_render_target()
{
    if (_dx_main_render_target_view_ptr)
    {
        _dx_main_render_target_view_ptr->Release();
        _dx_main_render_target_view_ptr = nullptr;
    }
}

void ImguiRendererDirectX::_clean_dx()
{
    this->_clean_render_target();
    if (_dx_swap_chain_ptr != nullptr)
    {
        _dx_swap_chain_ptr->Release();
        _dx_swap_chain_ptr = nullptr;
    }
    if (_dx_device_context_ptr != nullptr)
    {
        _dx_device_context_ptr->Release();
        _dx_device_context_ptr = nullptr;
    }
    if (_dx_device_ptr != nullptr)
    {
        _dx_device_ptr->Release();
        _dx_device_ptr = nullptr;
    }
}

} // namespace sihd::imgui