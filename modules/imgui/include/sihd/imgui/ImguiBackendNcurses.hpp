#ifndef __SIHD_IMGUI_IMGUIBACKENDNCURSES_HPP__
#define __SIHD_IMGUI_IMGUIBACKENDNCURSES_HPP__

#include <imgui.h>

#include <array>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include <sihd/imgui/IImguiBackend.hpp>

struct ImGuiContext;
struct ImGuiIO;
struct ImGuiPlatformImeData;
struct ImGuiViewport;
enum ImGuiKey : int;
using ImGuiKeyChord = int;

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
        // imgui units per terminal cell, same as the renderer's
        void set_cell_size(ImVec2 cell_size);

        void new_frame() override;
        bool should_close() override;
        void poll() override;
        void pre_render() override;
        void post_render() override;
        void shutdown() override;
        void terminate() override;

    private:
        struct MouseButton
        {
                bool down = false;
                bool pressed_this_poll = false;
                // a same-poll press+release is held one poll so imgui sees the button down for a full frame
                bool pending_release = false;
        };

        static void _set_ime_data(ImGuiContext *ctx, ImGuiViewport *viewport, ImGuiPlatformImeData *data);
        static const char *_get_clipboard_text(ImGuiContext *ctx);
        static void _set_clipboard_text(ImGuiContext *ctx, const char *text);

        void _load_terminal_keys();
        bool _process_input(int key, bool function_key, ImGuiIO & io);
        void _process_mouse_event(ImGuiIO & io);
        void _release_button(ImGuiIO & io, int button);
        bool _read_alt_key(int & key, bool & function_key);
        bool _process_key_event(int key, bool function_key, ImGuiIO & io, bool alt);
        ImGuiKeyChord _key_chord(int key, bool function_key) const;
        void _press_chord(ImGuiIO & io, ImGuiKeyChord chord);
        void _press_key(ImGuiIO & io, ImGuiKey key);
        bool _must_wait(ImGuiKey key) const;
        bool _virtual_pixels() const;
        int _idle_timeout_ms() const;

        bool _is_init;
        bool _should_close;
        bool _mouse_seen; // avoids hover at 0,0 before the first real mouse event
        bool _ctrl_from_mouse;
        bool _ime_visible;
        bool _in_paste;
        std::unordered_map<int, ImGuiKeyChord> _modified_keys;
        std::string _clipboard;
        std::vector<ImGuiKey> _pending_key_releases;
        std::vector<ImGuiKey> _released_keys;
        // in imgui's order: left, right, middle
        std::array<MouseButton, 3> _buttons;
        int _mx, _my;
        // a drag started on the drawn resize grip is reported shifted onto imgui's grip hit zone
        float _grip_dx, _grip_dy;
        int _ime_x, _ime_y;
        ImVec2 _cell_size;
        unsigned long _mstate;
        double _idle_frequency;
        std::chrono::steady_clock::time_point _last_time;
        std::chrono::steady_clock::time_point _frame_start;
        std::chrono::steady_clock::time_point _last_activity;
};

} // namespace sihd::imgui

#endif
