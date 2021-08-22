#ifndef __SIHD_UTIL_DEVICE_HPP__
# define __SIHD_UTIL_DEVICE_HPP__

# include <sihd/util/ServiceController.hpp>
# include <sihd/util/AService.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/IObserver.hpp>
# include <sihd/core/AChannelContainer.hpp>

namespace sihd::core
{

NEW_LOGGER("sihd::core");

using namespace sihd::util;

class Device:   public AChannelContainer,
                virtual public AService,
                virtual public IObserver<ServiceController>
{
    public:
        Device(const std::string & name, Node *parent = nullptr);
        virtual ~Device();

        virtual IServiceController *get_service_ctrl() override { return &_service_controller; }

        virtual void    observable_changed([[maybe_unused]] Channel *c) {}
        virtual void    observable_changed([[maybe_unused]] ServiceController *ctrl) {}

    protected:
        virtual bool    on_setup() override;
        virtual bool    on_init() override;
        virtual bool    on_start() override;
        virtual bool    on_stop() override;
        virtual bool    on_reset() override;

    private:
        ServiceController   _service_controller;
};

}

#endif 