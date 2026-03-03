#include <sihd/util/Logger.hpp>

#include <sihd/core/Device.hpp>

namespace sihd::core
{

using namespace sihd::util;

SIHD_LOGGER;

namespace
{

template <typename Callable>
bool for_each_child_service(Node *node, Callable && fn)
{
    bool ret = true;
    for (const std::string & child_name : node->children_keys())
    {
        Named *child = node->get_child(child_name);
        AService *service = dynamic_cast<AService *>(child);
        if (service != nullptr && fn(service, child_name) == false)
            ret = false;
    }
    return ret;
}

} // namespace

// permits copying of existent maps instead of instanciating every time the same maps
sihd::util::ServiceController Device::_default_service_controller;

Device::Device(const std::string & name, Node *parent):
    AChannelContainer(name, parent),
    _service_controller(_default_service_controller.statemachine)
{
    _service_controller.optional_setup();
}

Device::~Device()
{
    if (this->parent() == nullptr)
    {
        if (_default_service_controller.statemachine.last_event() == AService::Start)
            this->stop();
        if (_default_service_controller.statemachine.last_event() == AService::Stop)
            this->reset();
    }
}

ServiceController::State Device::device_state() const
{
    return _service_controller.state();
}

const char *Device::device_state_str() const
{
    return ServiceController::state_str(this->device_state());
}

bool Device::do_setup()
{
    bool ret = for_each_child_service(this, [this](AService *service, const std::string & child_name) {
        if (service->setup() == false)
        {
            SIHD_LOG(error, "Device: {} could not setup service: {}", this->name(), child_name);
            return false;
        }
        return true;
    });
    return ret && this->on_setup();
}

bool Device::do_init()
{
    bool ret = for_each_child_service(this, [this](AService *service, const std::string & child_name) {
        if (service->init() == false)
        {
            SIHD_LOG(error, "Device: {} could not init service: {}", this->name(), child_name);
            return false;
        }
        return true;
    });
    return ret && this->on_init();
}

bool Device::do_start()
{
    bool ret = true;
    std::list<AService *> started_services;
    for (const std::string & child_name : this->children_keys())
    {
        Named *child = this->get_child(child_name);
        AService *service = dynamic_cast<AService *>(child);
        if (service != nullptr)
        {
            if (service->start() == false)
            {
                SIHD_LOG(error, "Device: {} could not start service: {}", this->name(), child_name);
                ret = false;
                break;
            }
            else
                started_services.push_back(service);
        }
    }
    ret = ret && this->resolve_links();
    if (ret)
        ret = this->on_start();
    if (ret == false)
    {
        for (AService *service : started_services)
        {
            service->stop();
        }
    }
    return ret;
}

bool Device::do_stop()
{
    this->remove_channels_observation();
    bool ret = for_each_child_service(this, [this](AService *service, const std::string & child_name) {
        if (service->stop() == false)
        {
            SIHD_LOG(error, "Device: {} could not stop service: {}", this->name(), child_name);
            return false;
        }
        return true;
    });
    return this->on_stop() && ret;
}

bool Device::do_reset()
{
    bool ret = for_each_child_service(this, [this](AService *service, const std::string & child_name) {
        if (service->reset() == false)
        {
            SIHD_LOG(error, "Device: {} could not reset service: {}", this->name(), child_name);
            return false;
        }
        return true;
    });
    this->remove_children();
    return this->on_reset() && ret;
}

} // namespace sihd::core
