#ifndef __SIHD_IMGUI_IMGUIBACKENDNCURSES_HPP__
#define __SIHD_IMGUI_IMGUIBACKENDNCURSES_HPP__

#include <chrono>
#include <vector>

#include <sihd/imgui/IImguiBackend.hpp>

struct ImGuiContext;
struct ImGuiIO;
struct ImGuiPlatformImeData;
struct ImGuiViewport;
enum ImGuiKey : int;

namespace sihd::imgui
{

class ImguiBackendNcurses: public sihd::imgui::IImguiBackend
{
    public:
        ImguiBackendNcurses();
        virtual ~ImguiBackendNcurses();

        bool init();
        // frame rate once the screen stayed still and no input came for a second: input wakes it at once
        bool set_idle_frequency(double hz);

        void new_frame() override;
        bool should_close() override;
        void poll() override;
        void pre_render() override;
        void post_render() override;
        void shutdown() override;
        void terminate() override;

    private:
        static void _set_ime_data(ImGuiContext *ctx, ImGuiViewport *viewport, ImGuiPlatformImeData *data);

        void _process_mouse_event(ImGuiIO & io, bool & pressed_l, bool & pressed_r);
        bool _process_key_event(int key, bool function_key, ImGuiIO & io);
        void _press_key(ImGuiIO & io, ImGuiKey key);
        bool _must_wait(ImGuiKey key) const;
        int _idle_timeout_ms() const;

        bool _is_init;
        bool _should_close;
        bool _mouse_seen; // avoids hover at 0,0 before the first real mouse event
        bool _ctrl_from_mouse;
        // a same-poll press+release is held one poll so imgui sees the button down for a full frame
        bool _pending_lrelease, _pending_rrelease;
        bool _ime_visible;
        std::vector<ImGuiKey> _pending_key_releases;
        std::vector<ImGuiKey> _released_keys;
        int _mx, _my;
        int _lbut, _rbut;
        // a drag started on the drawn resize grip is reported shifted onto imgui's grip hit zone
        float _grip_dx, _grip_dy;
        int _ime_x, _ime_y;
        unsigned long _mstate;
        double _idle_frequency;
        std::chrono::steady_clock::time_point _last_time;
        std::chrono::steady_clock::time_point _frame_start;
        std::chrono::steady_clock::time_point _last_activity;
};

} // namespace sihd::imgui

#endif
