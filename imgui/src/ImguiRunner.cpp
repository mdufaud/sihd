#include <sihd/imgui/ImguiRunner.hpp>

#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/os.hpp>
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
    _running(false),
    _gui_running(false),
    _imgui_renderer_ptr(nullptr),
    _imgui_backend_ptr(nullptr)
{
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
    if (_imgui_backend_ptr == nullptr || _imgui_renderer_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiRunner: backend and renderer must be set before run()");
        return false;
    }
    {
        std::lock_guard l(_mutex);
        if (_running)
            return true;
        _running = true;
    }
    if constexpr (util::os::is_emscripten)
    {
#if defined(__SIHD_EMSCRIPTEN__)
        constexpr int emscripten_fps = 0;
        constexpr bool infinite_loop = true;
        emscripten_set_main_loop_arg(ImguiRunner::_emscripten_loop, this, emscripten_fps, infinite_loop);
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
#if defined(__SIHD_EMSCRIPTEN__)
    if constexpr (util::os::is_emscripten)
        emscripten_cancel_main_loop();
#endif
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
    if constexpr (util::os::is_emscripten)
    {
        if (!_running || !_gui_running)
            this->_shutdown();
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
    // Official ImGui order: renderer first, then backend
    _imgui_renderer_ptr->shutdown();
    _imgui_backend_ptr->shutdown();
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

} // namespace sihd::imgui