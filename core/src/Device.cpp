#include <sihd/core/Device.hpp>

namespace sihd::core
{

LOGGER;

Device::Device(const std::string & name, Node *parent): ChannelContainer(name, parent)
{
    _service_controller.optionnal_setup();
}

Device::~Device()
{
}

#define WATERFALL_SERVICE_OPERATION(OP) \
bool    Device::on_##OP()\
{\
    for (auto & [name, child]: this->get_children())\
    {\
        (void)name;\
        AService *service = dynamic_cast<AService *>(child);\
        if (service != nullptr && service->OP() == false)\
        {\
            LOG(error, "Device: " << this->get_name() << " << could not " #OP " service: " << name);\
            return false;\
        }\
    }\
    return true;\
}

WATERFALL_SERVICE_OPERATION(setup);
//WATERFALL_SERVICE_OPERATION(init);
WATERFALL_SERVICE_OPERATION(start);
WATERFALL_SERVICE_OPERATION(stop);
WATERFALL_SERVICE_OPERATION(reset);

bool    Device::on_init()
{
    for (auto & [name, child]: this->get_children())
    {
        (void)name;
        AService *service = dynamic_cast<AService *>(child);
        if (service != nullptr && service->init() == false)
        {
            LOG(error, "Device: " << this->get_name() << " << could not init service: " << name);
            return false;
        }
    }
    return this->resolve_links();
}

}