#ifndef __SIHD_IMGUI_IMGUIRUNNER_HPP__
#define __SIHD_IMGUI_IMGUIRUNNER_HPP__

#include <functional>
#include <mutex>

#include <sihd/imgui/IImguiBackend.hpp>
#include <sihd/imgui/IImguiRenderer.hpp>
#include <sihd/util/Named.hpp>

namespace sihd::imgui
{

class ImguiRunner: public sihd::util::Named
{
    public:
        ImguiRunner(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~ImguiRunner();

        bool run();
        bool stop();
        bool is_running() const;

        virtual bool init_imgui();

        void set_renderer(IImguiRenderer *renderer);
        void set_backend(IImguiBackend *backend);
        void set_build_frame(std::function<bool()> method);

        bool set_emscripten(bool active);

    protected:
        static void _emscripten_loop(void *arg);

        virtual void _loop();
        virtual void _loop_once();
        virtual bool _new_frame();
        virtual bool _build_frame();
        virtual bool _render();
        virtual void _shutdown();

    private:
        std::mutex _mutex;

        bool _running;
        bool _gui_running;
        bool _emscripten;
        int _emscripten_fps;
        IImguiRenderer *_imgui_renderer_ptr;
        IImguiBackend *_imgui_backend_ptr;

        std::function<bool()> _build_frame_method;
};

} // namespace sihd::imgui

#endif