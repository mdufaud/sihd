#include <sihd/core/Device.hpp>

namespace sihd::core
{

LOGGER;

Device::Device(const std::string & name, Node *parent): AChannelContainer(name, parent)
{
    _service_controller.optionnal_setup();
}

Device::~Device()
{
}

#define WATERFALL_SERVICE_OPERATION(OP) \
bool    Device::do_##OP()\
{\
    for (auto & [name, entry]: this->get_children())\
    {\
        AService *service = dynamic_cast<AService *>(entry->obj);\
        if (service != nullptr && service->OP() == false)\
        {\
            LOG(error, "Device: " << this->get_name() << " << could not " #OP " service: " << name);\
            return false;\
        }\
    }\
    return this->on_##OP();\
}

WATERFALL_SERVICE_OPERATION(setup);
WATERFALL_SERVICE_OPERATION(init);

bool    Device::do_start()
{
    bool ret = true;
    std::list<AService *> started_services;
    for (auto & [name, entry]: this->get_children())
    {
        AService *service = dynamic_cast<AService *>(entry->obj);
        if (service != nullptr)
        {
            if (service->start() == false)
            {
                LOG(error, "Device: " << this->get_name() << " << could not start service: " << name);
                ret = false;
                break ;
            }
            started_services.push_back(service);
        }
    }
    ret = this->resolve_links();
    if (ret == false)
    {
        // return started service to stop state if failed
        for (AService *service: started_services)
        {
            service->stop();
        }
    }
    return ret && this->on_start();
}

bool    Device::do_stop()
{
    this->remove_channels_observation();
    for (auto & [name, entry]: this->get_children())
    {
        AService *service = dynamic_cast<AService *>(entry->obj);
        if (service != nullptr && service->stop() == false)
        {
            LOG(error, "Device: " << this->get_name() << " << could not stop service: " << name);
            return false;
        }
    }
    return this->on_stop();
}

bool    Device::do_reset()
{
    for (auto & [name, entry]: this->get_children())
    {
        AService *service = dynamic_cast<AService *>(entry->obj);
        if (service != nullptr && service->reset() == false)
        {
            LOG(error, "Device: " << this->get_name() << " << could not reset service: " << name);
            return false;
        }
    }
    this->delete_children();
    return this->on_reset();
}

}