#ifndef __SIHD_TUI_DEVSCREEN_HPP__
#define __SIHD_TUI_DEVSCREEN_HPP__

#include <sihd/core/Device.hpp>
#include <sihd/tui/Refresher.hpp>

namespace sihd::tui
{

class DevScreen: public sihd::core::Device
{
    public:
        DevScreen(const std::string & name, sihd::util::Node *parent = nullptr);
        ~DevScreen() override;

        bool is_running() const override;

        // not owned, outlives the run of the device
        void set_refresher(Refresher *refresher);

    protected:
        using sihd::core::Device::handle;

        void handle(sihd::core::Channel *channel) override;

        bool on_start() override;
        bool on_stop() override;

    private:
        bool _running;
        Refresher *_refresher;
};

} // namespace sihd::tui

#endif
