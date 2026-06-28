#include <sihd/imgui/ImguiBackendGlfw.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::imgui
{

SIHD_LOGGER;

// TODO
static void glfw_error_callback(int error, const char *description)
{
    SIHD_LOG(error, "GLFW: {} ({})", description, error);
}

ImguiBackendGlfw::ImguiBackendGlfw(): _glfw_window_ptr(nullptr)
{
    _is_init = false;
}

ImguiBackendGlfw::~ImguiBackendGlfw()
{
    this->shutdown();
    this->terminate();
}

bool ImguiBackendGlfw::init_window(const std::string & name, size_t width, size_t height)
{
    if (_glfw_window_ptr != nullptr)
    {
        SIHD_LOG(warning, "ImguiBackendGlfw: already initialized");
        return true;
    }
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;
    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    // Create window with graphics context
    _glfw_window_ptr = glfwCreateWindow(width, height, name.c_str(), NULL, NULL);
    if (_glfw_window_ptr == NULL)
        return false;
    glfwMakeContextCurrent(_glfw_window_ptr);
    glfwSwapInterval(1); // Enable vsync
    return true;
}

bool ImguiBackendGlfw::init_backend_opengl()
{
    if (_glfw_window_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendGlfw: window not initialized");
        return false;
    }
    _is_init = ImGui_ImplGlfw_InitForOpenGL(_glfw_window_ptr, true);
    return _is_init;
}

void ImguiBackendGlfw::new_frame()
{
    ImGui_ImplGlfw_NewFrame();
}

void ImguiBackendGlfw::poll()
{
    glfwPollEvents();
}

bool ImguiBackendGlfw::should_close()
{
    return glfwWindowShouldClose(_glfw_window_ptr);
}

void ImguiBackendGlfw::pre_render() {}

void ImguiBackendGlfw::post_render()
{
    glfwSwapBuffers(_glfw_window_ptr);
}

void ImguiBackendGlfw::shutdown()
{
    if (_is_init)
    {
        ImGui_ImplGlfw_Shutdown();
        _is_init = false;
    }
}

void ImguiBackendGlfw::terminate()
{
    if (_glfw_window_ptr != nullptr)
    {
        glfwDestroyWindow(_glfw_window_ptr);
        _glfw_window_ptr = nullptr;
        glfwTerminate();
    }
}

} // namespace sihd::imgui
