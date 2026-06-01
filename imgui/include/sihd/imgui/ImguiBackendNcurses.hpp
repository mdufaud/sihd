#ifndef __SIHD_IMGUI_IMGUIBACKENDNCURSES_HPP__
#define __SIHD_IMGUI_IMGUIBACKENDNCURSES_HPP__

#include <ncurses.h>

#include <chrono>

#include <sihd/imgui/IImguiBackend.hpp>

struct ImGuiIO;

namespace sihd::imgui
{

class ImguiBackendNcurses: public sihd::imgui::IImguiBackend
{
    public:
        ImguiBackendNcurses();
        virtual ~ImguiBackendNcurses();

        bool init();

        void new_frame() override;
        bool should_close() override;
        void poll() override;
        void pre_render() override;
        void post_render() override;
        void shutdown() override;
        void terminate() override;

    private:
        // ── Per-frame helpers ─────────────────────────────────────────────────
        void _process_mouse_event(ImGuiIO & io, bool & pressed_l, bool & pressed_r);
        bool _process_key_event(int key, ImGuiIO & io);

        // ── State ─────────────────────────────────────────────────────────────
        bool _is_init;
        bool _should_close;
        bool _mouse_seen;      // true after first real mouse event (avoids hover at 0,0 on startup)
        bool _ctrl_from_mouse; // last emitted Ctrl state from mouse modifier bits
        // Deferred button releases: when a press+release land in the same poll, the
        // release is held one poll so imgui sees the button down for a full frame.
        bool _pending_lrelease, _pending_rrelease;
        // Persistent mouse state (mirrors imtui static mx/my/lbut/rbut/mstate)
        int _mx, _my;
        int _lbut, _rbut;
        unsigned long _mstate;
        std::chrono::steady_clock::time_point _last_time;
        std::chrono::steady_clock::time_point _frame_start;
};

} // namespace sihd::imgui

#endif
