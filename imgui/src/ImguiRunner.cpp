#include <sihd/imgui/ImguiRunner.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/platform.hpp>

#if defined(__SIHD_EMSCRIPTEN__)
# include <emscripten.h>
#endif

namespace sihd::imgui
{

SIHD_UTIL_REGISTER_FACTORY(ImguiRunner)

SIHD_NEW_LOGGER("sihd::imgui");

ImguiRunner::ImguiRunner(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent),
    _imgui_renderer_ptr(nullptr),
    _imgui_backend_ptr(nullptr)
{
    _running = false;
    _emscripten = false;
    _emscripten_fps = 0;
}

ImguiRunner::~ImguiRunner()
{
    this->stop();
}

void ImguiRunner::_emscripten_loop(void *arg)
{
    if (arg != nullptr)
    {
        ImguiRunner *imgui = (ImguiRunner *)arg;
        imgui->_loop_once();
    }
}

bool ImguiRunner::run()
{
    {
        std::lock_guard l(_mutex);
        if (_running)
            return true;
        _running = true;
    }
    if (_emscripten)
    {
#if defined(__SIHD_EMSCRIPTEN__)
        emscripten_set_main_loop_arg(ImguiRunner::_emscripten_loop, this, _emscripten_fps, true);
#endif
    }
    else
    {
        this->_loop();
    }
    return _running;
}

bool ImguiRunner::stop()
{
    {
        std::lock_guard l(_mutex);
        if (_running == false)
            return true;
        _running = false;
    }
    return _running == false;
}

bool ImguiRunner::is_running() const
{
    return _running;
}

bool ImguiRunner::init_imgui()
{
    IMGUI_CHECKVERSION();
    ImGuiContext *ctx = ImGui::CreateContext();
    if (ctx == nullptr)
        return false;
    ImGuiIO & io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();
    // ImGui::StyleColorsLight();
    return true;
}

void ImguiRunner::_loop_once()
{
    _imgui_backend_ptr->poll();
    _gui_running = _imgui_backend_ptr->should_close() == false;
    if (_running && _gui_running)
    {
        _gui_running = this->_new_frame();
        _gui_running = _gui_running && this->_build_frame();
        _gui_running = _gui_running && this->_render();
    }
}

void ImguiRunner::_loop()
{
    _gui_running = true;
    while (_running && _gui_running)
        this->_loop_once();
    this->_shutdown();
}

bool ImguiRunner::_new_frame()
{
    _imgui_renderer_ptr->new_frame();
    _imgui_backend_ptr->new_frame();
    ImGui::NewFrame();
    return true;
}

bool ImguiRunner::_build_frame()
{
    if (_build_frame_method)
        return _build_frame_method();
    return false;
}

bool ImguiRunner::_render()
{
    ImGui::Render();
    _imgui_backend_ptr->pre_render();
    _imgui_renderer_ptr->render(ImGui::GetDrawData());
    _imgui_backend_ptr->post_render();
    return true;
}

void ImguiRunner::_shutdown()
{
    _imgui_backend_ptr->shutdown();
    _imgui_renderer_ptr->shutdown();
    ImGui::DestroyContext();
    _imgui_backend_ptr->terminate();
}

void ImguiRunner::set_renderer(IImguiRenderer *renderer)
{
    _imgui_renderer_ptr = renderer;
}

void ImguiRunner::set_backend(IImguiBackend *backend)
{
    _imgui_backend_ptr = backend;
}

void ImguiRunner::set_build_frame(std::function<bool()> method)
{
    _build_frame_method = std::move(method);
}

bool ImguiRunner::set_emscripten(bool active)
{
#if !defined(__SIHD_EMSCRIPTEN__)
    (void)active;
    SIHD_LOG(warning, "ImguiRunner: cannot set emscripten - it is absent");
    return false;
#else
    _emscripten = active;
    return true;
#endif
}

} // namespace sihd::imgui