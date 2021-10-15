#ifndef __SIHD_UTIL_DEVICE_HPP__
# define __SIHD_UTIL_DEVICE_HPP__

# include <sihd/util/ServiceController.hpp>
# include <sihd/util/AService.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/IObserver.hpp>
# include <sihd/core/AChannelContainer.hpp>

namespace sihd::core
{

class Device:   public AChannelContainer,
                virtual public sihd::util::AService,
                virtual public sihd::util::IObserver<sihd::util::ServiceController>
{
    public:
        Device(const std::string & name, Node *parent = nullptr);
        virtual ~Device();

        virtual sihd::util::AService::IServiceController *get_service_ctrl() override { return &_service_controller; }

    protected:
        virtual void observable_changed([[maybe_unused]] Channel *c) override {}
        virtual void observable_changed([[maybe_unused]] sihd::util::ServiceController *ctrl) override {}

        virtual bool do_setup() override;
        virtual bool do_init() override;
        virtual bool do_start() override;
        virtual bool do_stop() override;
        virtual bool do_reset() override;

        virtual bool on_setup() { return true; };
        virtual bool on_init() { return true; };
        virtual bool on_start() { return true; };
        virtual bool on_stop() { return true; };
        virtual bool on_reset() { return true; };

    private:
        sihd::util::ServiceController _service_controller;
        static sihd::util::ServiceController _default_service_controller;
};

}

#endif 