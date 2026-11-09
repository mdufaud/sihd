
#include <sihd/core/Device.hpp>
#include <sihd/util/Logger.hpp>

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
        // a link is an alias whose target may already be gone, never a child to drive
        if (node->is_link(child_name))
            continue;
        AService *service = dynamic_cast<AService *>(node->get_child(child_name));
        if (service != nullptr && fn(service, child_name) == false)
            ret = false;
    }
    return ret;
}

} // namespace

Device::Device(const std::string & name, Node *parent): AChannelContainer(name, parent)
{
    _service_controller.optional_setup();
}

Device::~Device()
{
    if (this->parent() == nullptr)
    {
        if (this->device_state() == ServiceController::Running)
            this->stop();
        // a failed start or stop leaves a state that refuses a reset
        if (this->device_state() == ServiceController::Stopped)
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
        if (this->is_link(child_name))
            continue;
        AService *service = dynamic_cast<AService *>(this->get_child(child_name));
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

    if (ret && SIHD_UNEXPECTED_LOG(this->resolve_links()))
        ret = false;
    if (ret)
        ret = this->on_start();

    if (ret == false)
    {
        // a refused start never reaches do_stop: what on_start observed must go now
        this->remove_channels_observation();
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

void Device::handle(Channel *) {}

void Device::handle(ServiceController *) {}

bool Device::on_setup()
{
    return true;
}

bool Device::on_init()
{
    return true;
}

bool Device::on_start()
{
    return true;
}

bool Device::on_stop()
{
    return true;
}

bool Device::on_reset()
{
    return true;
}

AService::IServiceController *Device::service_ctrl()
{
    return &_service_controller;
}

} // namespace sihd::core
