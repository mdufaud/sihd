#include <sihd/bt/Device.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/str.hpp>

#include "address.hpp"

namespace sihd::bt
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(Device)

void Device::_add_confs()
{
    this->set_adapter(0);
    _last_event = Event::disconnected;
    this->add_conf("address", &Device::set_address);
    this->add_conf("adapter", &Device::set_adapter);
}

bool Device::set_address(const std::string & address)
{
    if (!parse_address(address))
        return false;
    _address = str::to_upper_copy(address);
    return true;
}

bool Device::set_adapter(size_t index)
{
    _adapter_index = index;
    return true;
}

void Device::set_pairing_handler(PairingHandler handler)
{
    _pairing_handler = std::move(handler);
}

Device::Event Device::last_event() const
{
    return _last_event;
}

std::optional<GattNotification> Device::last_notification() const
{
    return _last_notification;
}

void Device::_publish_event(Event event)
{
    _last_event = event;
    this->notify_observers(this);
}

void Device::_queue_notification(GattNotification && notification)
{
    notification.timestamp = Clock::default_clock.now();
    {
        std::lock_guard lock(_pending_mutex);
        _pending.emplace_back(std::move(notification));
    }
    this->_wake_loop();
}

void Device::_flush_notifications()
{
    while (true)
    {
        {
            std::lock_guard lock(_pending_mutex);
            if (_pending.empty())
                return;
            _last_notification = std::move(_pending.front());
            _pending.pop_front();
        }
        this->_publish_event(Event::notification);
    }
}

} // namespace sihd::bt
