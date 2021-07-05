#ifndef __SIHD_UTIL_DEVICE_HPP__
# define __SIHD_UTIL_DEVICE_HPP__

# include <sihd/util/ServiceController.hpp>
# include <sihd/util/AService.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/Node.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IObserver.hpp>

namespace sihd::util
{

class Device:   public Node,
                public Configurable,
                virtual public AService,
                virtual public IObserver<ServiceController>
{
    public:
        Device(const std::string & name, Node *parent = nullptr);
        virtual ~Device();

        virtual IController *get_ctrl() override { return &_service_controller; }

        virtual bool    on_setup();
        virtual bool    on_init();
        virtual bool    on_start();
        virtual bool    on_stop();
        virtual bool    on_reset();

    protected:
    
    private:
        ServiceController   _service_controller;
};

}

#endif 