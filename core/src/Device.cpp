#include <sihd/core/Device.hpp>

namespace sihd::core
{

using namespace sihd::util;

SIHD_LOGGER;

// permits copying of existent maps instead of instanciating every time the same maps
sihd::util::ServiceController Device::_default_service_controller;

Device::Device(const std::string & name, Node *parent):
    AChannelContainer(name, parent), _service_controller(_default_service_controller.statemachine)
{
    _service_controller.optionnal_setup();
}

Device::~Device()
{
}

bool    Device::do_setup()
{
    for (auto & [name, entry]: this->get_children())
    {
        AService *service = dynamic_cast<AService *>(entry->obj);
        if (service != nullptr && service->setup() == false)
        {
            SIHD_LOG(error, "Device: " << this->get_name() << " could not setup service: " << name);
            return false;
        }
    }
    return this->on_setup();
}

bool    Device::do_init()
{
    for (auto & [name, entry]: this->get_children())
    {
        AService *service = dynamic_cast<AService *>(entry->obj);
        if (service != nullptr && service->init() == false)
        {
            SIHD_LOG(error, "Device: " << this->get_name() << " could not init service: " << name);
            return false;
        }
    }
    return this->on_init();
}

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
                SIHD_LOG(error, "Device: " << this->get_name() << " could not start service: " << name);
                ret = false;
                break ;
            }
            else
                started_services.push_back(service);
        }
    }
    ret = ret && this->resolve_links();
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
            SIHD_LOG(error, "Device: " << this->get_name() << " could not stop service: " << name);
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
            SIHD_LOG(error, "Device: " << this->get_name() << " could not reset service: " << name);
            return false;
        }
    }
    this->delete_children();
    return this->on_reset();
}

}