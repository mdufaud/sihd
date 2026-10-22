#ifndef __SIHD_IMGUI_IMGUIBACKENDANDROID_HPP__
#define __SIHD_IMGUI_IMGUIBACKENDANDROID_HPP__

#include <android_native_app_glue.h>
#include <imgui_impl_android.h>

#include <string>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/configuration.h>
#include <android/log.h>

#include <sihd/imgui/IImguiBackend.hpp>

namespace sihd::imgui
{

class ImguiBackendAndroid: public sihd::imgui::IImguiBackend
{
    public:
        ImguiBackendAndroid();
        virtual ~ImguiBackendAndroid();

        void set_app(struct android_app *app);

        // blocks until the native window is available
        bool init_egl();

        bool init_backend();

        void new_frame();
        bool should_close();
        void poll();
        void pre_render();
        void post_render();
        void shutdown();
        void terminate();

        ANativeWindow *window();

    protected:

    private:
        bool _init_egl_context();
        void _shutdown_egl();
        int _show_soft_keyboard_input();
        int _poll_unicode_chars();

        static void _handle_app_cmd(struct android_app *app, int32_t cmd);
        static int32_t _handle_input_event(struct android_app *app, AInputEvent *event);

        bool _is_init;
        bool _egl_ready;
        bool _want_text_input_last;
        struct android_app *_app;
        ANativeWindow *_window_ptr;
        EGLDisplay _egl_display;
        EGLSurface _egl_surface;
        EGLContext _egl_context;
        std::string _ini_filename;
};

} // namespace sihd::imgui

#endif
