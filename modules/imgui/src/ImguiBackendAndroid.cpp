#include <sihd/imgui/ImguiBackendAndroid.hpp>
#include <sihd/util/Logger.hpp>

#include <imgui.h>

namespace sihd::imgui
{

SIHD_LOGGER;

ImguiBackendAndroid::ImguiBackendAndroid():
    _is_init(false),
    _egl_ready(false),
    _app(nullptr),
    _window_ptr(nullptr),
    _egl_display(EGL_NO_DISPLAY),
    _egl_surface(EGL_NO_SURFACE),
    _egl_context(EGL_NO_CONTEXT)
{
}

ImguiBackendAndroid::~ImguiBackendAndroid()
{
    this->shutdown();
    this->terminate();
}

void ImguiBackendAndroid::set_app(struct android_app *app)
{
    _app = app;
    _app->userData = this;
    _app->onAppCmd = ImguiBackendAndroid::_handle_app_cmd;
    _app->onInputEvent = ImguiBackendAndroid::_handle_input_event;
}

void ImguiBackendAndroid::_handle_app_cmd(struct android_app *app, int32_t cmd)
{
    auto *backend = static_cast<ImguiBackendAndroid *>(app->userData);
    switch (cmd)
    {
        case APP_CMD_INIT_WINDOW:
            if (backend->_app->window != nullptr && !backend->_egl_ready)
            {
                backend->_init_egl_context();
            }
            break;
        case APP_CMD_TERM_WINDOW:
            backend->_shutdown_egl();
            break;
        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_LOST_FOCUS:
            break;
    }
}

int32_t ImguiBackendAndroid::_handle_input_event(struct android_app * /* app */, AInputEvent *event)
{
    return ImGui_ImplAndroid_HandleInputEvent(event);
}

bool ImguiBackendAndroid::_init_egl_context()
{
    _window_ptr = _app->window;
    ANativeWindow_acquire(_window_ptr);

    _egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (_egl_display == EGL_NO_DISPLAY)
    {
        SIHD_LOG(error, "ImguiBackendAndroid: eglGetDisplay returned EGL_NO_DISPLAY");
        return false;
    }

    if (eglInitialize(_egl_display, 0, 0) != EGL_TRUE)
    {
        SIHD_LOG(error, "ImguiBackendAndroid: eglInitialize failed");
        return false;
    }

    const EGLint egl_attributes[] = {EGL_BLUE_SIZE,
                                     8,
                                     EGL_GREEN_SIZE,
                                     8,
                                     EGL_RED_SIZE,
                                     8,
                                     EGL_DEPTH_SIZE,
                                     24,
                                     EGL_SURFACE_TYPE,
                                     EGL_WINDOW_BIT,
                                     EGL_NONE};

    EGLint num_configs = 0;
    if (eglChooseConfig(_egl_display, egl_attributes, nullptr, 0, &num_configs) != EGL_TRUE)
    {
        SIHD_LOG(error, "ImguiBackendAndroid: eglChooseConfig failed");
        return false;
    }
    if (num_configs == 0)
    {
        SIHD_LOG(error, "ImguiBackendAndroid: eglChooseConfig returned 0 configs");
        return false;
    }

    EGLConfig egl_config;
    eglChooseConfig(_egl_display, egl_attributes, &egl_config, 1, &num_configs);

    EGLint egl_format;
    eglGetConfigAttrib(_egl_display, egl_config, EGL_NATIVE_VISUAL_ID, &egl_format);
    ANativeWindow_setBuffersGeometry(_window_ptr, 0, 0, egl_format);

    const EGLint egl_context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    _egl_context = eglCreateContext(_egl_display, egl_config, EGL_NO_CONTEXT, egl_context_attributes);
    if (_egl_context == EGL_NO_CONTEXT)
    {
        SIHD_LOG(error, "ImguiBackendAndroid: eglCreateContext returned EGL_NO_CONTEXT");
        return false;
    }

    _egl_surface = eglCreateWindowSurface(_egl_display, egl_config, _window_ptr, nullptr);
    eglMakeCurrent(_egl_display, _egl_surface, _egl_surface, _egl_context);

    _egl_ready = true;
    SIHD_LOG(info, "ImguiBackendAndroid: EGL initialized");
    return true;
}

bool ImguiBackendAndroid::init_egl()
{
    if (_app == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendAndroid: set_app() must be called before init_egl()");
        return false;
    }

    // Poll until the native window is available (APP_CMD_INIT_WINDOW fires)
    while (!_egl_ready)
    {
        int out_events;
        struct android_poll_source *out_data;
        while (ALooper_pollOnce(100, nullptr, &out_events, (void **)&out_data) >= 0)
        {
            if (out_data != nullptr)
                out_data->process(_app, out_data);
        }
        if (_app->destroyRequested)
            return false;
    }
    return true;
}

bool ImguiBackendAndroid::init_backend()
{
    if (!_egl_ready || _window_ptr == nullptr)
    {
        SIHD_LOG(error, "ImguiBackendAndroid: EGL must be initialized before backend");
        return false;
    }

    _is_init = ImGui_ImplAndroid_Init(_window_ptr);
    if (!_is_init)
        return false;

    // Save imgui.ini to app internal storage
    _ini_filename = std::string(_app->activity->internalDataPath) + "/imgui.ini";
    ImGuiIO & io = ImGui::GetIO();
    io.IniFilename = _ini_filename.c_str();

    // DPI scaling based on screen density
    ImGuiStyle & style = ImGui::GetStyle();
    float scale = 2.0f;
    if (_app->config != nullptr)
    {
        int32_t density = AConfiguration_getDensity(_app->config);
        if (density >= ACONFIGURATION_DENSITY_XXXHIGH)
            scale = 4.0f;
        else if (density >= ACONFIGURATION_DENSITY_XXHIGH)
            scale = 3.0f;
        else if (density >= ACONFIGURATION_DENSITY_XHIGH)
            scale = 2.0f;
        else if (density >= ACONFIGURATION_DENSITY_HIGH)
            scale = 1.5f;
    }
    style.ScaleAllSizes(scale);
    io.FontGlobalScale = scale;

    SIHD_LOG(info, "ImguiBackendAndroid: backend initialized (scale: {})", scale);
    return true;
}

void ImguiBackendAndroid::poll()
{
    int out_events;
    struct android_poll_source *out_data;
    while (ALooper_pollOnce(_egl_ready ? 0 : -1, nullptr, &out_events, (void **)&out_data) >= 0)
    {
        if (out_data != nullptr)
            out_data->process(_app, out_data);
        if (_app->destroyRequested)
            return;
    }
}

void ImguiBackendAndroid::new_frame()
{
    if (!_egl_ready)
        return;

    _poll_unicode_chars();

    static bool want_text_input_last = false;
    ImGuiIO & io = ImGui::GetIO();
    if (io.WantTextInput && !want_text_input_last)
        _show_soft_keyboard_input();
    want_text_input_last = io.WantTextInput;

    ImGui_ImplAndroid_NewFrame();
}

bool ImguiBackendAndroid::should_close()
{
    return _app != nullptr && _app->destroyRequested != 0;
}

void ImguiBackendAndroid::pre_render() {}

void ImguiBackendAndroid::post_render()
{
    if (_egl_ready)
        eglSwapBuffers(_egl_display, _egl_surface);
}

void ImguiBackendAndroid::shutdown()
{
    if (_is_init)
    {
        ImGui_ImplAndroid_Shutdown();
    }
    _is_init = false;
}

void ImguiBackendAndroid::terminate()
{
    _shutdown_egl();
}

void ImguiBackendAndroid::_shutdown_egl()
{
    if (_egl_display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (_egl_context != EGL_NO_CONTEXT)
            eglDestroyContext(_egl_display, _egl_context);
        if (_egl_surface != EGL_NO_SURFACE)
            eglDestroySurface(_egl_display, _egl_surface);
        eglTerminate(_egl_display);
    }
    _egl_display = EGL_NO_DISPLAY;
    _egl_context = EGL_NO_CONTEXT;
    _egl_surface = EGL_NO_SURFACE;
    if (_window_ptr != nullptr)
    {
        ANativeWindow_release(_window_ptr);
        _window_ptr = nullptr;
    }
    _egl_ready = false;
}

int ImguiBackendAndroid::_show_soft_keyboard_input()
{
    JavaVM *java_vm = _app->activity->vm;
    JNIEnv *java_env = nullptr;

    jint jni_return = java_vm->GetEnv((void **)&java_env, JNI_VERSION_1_6);
    if (jni_return == JNI_ERR)
        return -1;

    jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
    if (jni_return != JNI_OK)
        return -2;

    jclass native_activity_clazz = java_env->GetObjectClass(_app->activity->clazz);
    if (native_activity_clazz == nullptr)
        return -3;

    jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "showSoftInput", "()V");
    if (method_id == nullptr)
        return -4;

    java_env->CallVoidMethod(_app->activity->clazz, method_id);

    jni_return = java_vm->DetachCurrentThread();
    if (jni_return != JNI_OK)
        return -5;

    return 0;
}

int ImguiBackendAndroid::_poll_unicode_chars()
{
    JavaVM *java_vm = _app->activity->vm;
    JNIEnv *java_env = nullptr;

    jint jni_return = java_vm->GetEnv((void **)&java_env, JNI_VERSION_1_6);
    if (jni_return == JNI_ERR)
        return -1;

    jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
    if (jni_return != JNI_OK)
        return -2;

    jclass native_activity_clazz = java_env->GetObjectClass(_app->activity->clazz);
    if (native_activity_clazz == nullptr)
        return -3;

    jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "pollUnicodeChar", "()I");
    if (method_id == nullptr)
        return -4;

    ImGuiIO & io = ImGui::GetIO();
    jint unicode_character;
    while ((unicode_character = java_env->CallIntMethod(_app->activity->clazz, method_id)) != 0)
        io.AddInputCharacter(unicode_character);

    jni_return = java_vm->DetachCurrentThread();
    if (jni_return != JNI_OK)
        return -5;

    return 0;
}

} // namespace sihd::imgui
