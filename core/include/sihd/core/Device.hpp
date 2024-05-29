#ifndef __SIHD_UTIL_DEVICE_HPP__
#define __SIHD_UTIL_DEVICE_HPP__

#include <sihd/core/AChannelContainer.hpp>
#include <sihd/core/ACoreService.hpp>

namespace sihd::core
{

class Device: public AChannelContainer,
              public ACoreService
{
    public:
        using ACoreService::init;
        using ACoreService::is_running;
        using ACoreService::reset;
        using ACoreService::setup;
        using ACoreService::start;
        using ACoreService::stop;

        Device(const std::string & name, Node *parent = nullptr);
        virtual ~Device();

        virtual sihd::util::ServiceController::State device_state() const;
        virtual const char *device_state_str() const;

    protected:
        virtual void handle([[maybe_unused]] Channel *c) override {}
        virtual void handle([[maybe_unused]] sihd::util::ServiceController *ctrl) override {}

        bool do_setup() override;
        bool do_init() override;
        bool do_start() override;
        bool do_stop() override;
        bool do_reset() override;

        virtual bool on_setup() { return true; };
        virtual bool on_init() { return true; };
        virtual bool on_start() { return true; };
        virtual bool on_stop() { return true; };
        virtual bool on_reset() { return true; };

        virtual sihd::util::AService::IServiceController *service_ctrl() override { return &_service_controller; }
        sihd::util::ServiceController _service_controller;

    private:
        static sihd::util::ServiceController _default_service_controller;
};

} // namespace sihd::core

#endif
