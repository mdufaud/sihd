#ifndef __SIHD_IMGUI_IIMGUIBACKEND_HPP__
#define __SIHD_IMGUI_IIMGUIBACKEND_HPP__

namespace sihd::imgui
{

class IImguiBackend
{
    public:
        virtual ~IImguiBackend() = default;

        virtual void new_frame() = 0;
        virtual bool should_close() = 0;
        virtual void poll() = 0;
        virtual void pre_render() = 0;
        virtual void post_render() = 0;
        virtual void shutdown() = 0;
        virtual void terminate() = 0;
};

} // namespace sihd::imgui

#endif