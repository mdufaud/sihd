#include <sihd/bt/Advertiser.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

#include "bluez.hpp"

namespace sihd::bt
{

using namespace sihd::util;
namespace dbus = sihd::sys::linux_::dbus;

SIHD_LOGGER;

namespace
{

constexpr ZStringView advertisement_path = "/org/sihd/bt/advertisement";
const Duration max_wait(time::hours(1));

dbus::ObjectServer::Interface advertisement_interface(const Advertisement & advertisement, bool & released)
{
    using Property = dbus::ObjectServer::Property;
    std::vector<Property> properties;
    properties.push_back(Property::constant("Type", advertisement.connectable ? "peripheral" : "broadcast"));
    if (!advertisement.local_name.empty())
        properties.push_back(Property::constant("LocalName", advertisement.local_name));
    if (!advertisement.service_uuids.empty())
        properties.push_back(Property::constant("ServiceUUIDs", advertisement.service_uuids));
    if (!advertisement.manufacturer_data.empty())
    {
        properties.push_back(
            Property::constant("ManufacturerData", dbus::Value::variant_dict(advertisement.manufacturer_data)));
    }
    if (!advertisement.service_data.empty())
        properties.push_back(Property::constant("ServiceData", dbus::Value::variant_dict(advertisement.service_data)));

    return dbus::ObjectServer::Interface {
        .name = std::string(bluez::advertisement_interface),
        .methods = {dbus::ObjectServer::Method {
            .name = "Release",
            .in = {},
            .out = {},
            .handler =
                [&released](dbus::ObjectServer::Call & call) {
                    released = true;
                    return call.reply();
                },
        }},
        .properties = std::move(properties),
        .signals = {},
    };
}

} // namespace

struct Advertiser::Impl
{
        Waitable waitable;
        std::function<void()> wake;
};

Advertiser::Advertiser(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>())
{
    this->set_adapter(0);
    this->add_conf("adapter", &Advertiser::set_adapter);
}

Advertiser::~Advertiser()
{
    if (this->is_running())
        this->stop();
}

bool Advertiser::on_start()
{
    auto connection = bluez::open_system_bus();
    if (SIHD_UNEXPECTED_LOG(connection))
        return false;
    auto adapter_path = bluez::adapter_path(*connection, _adapter_index);
    if (SIHD_UNEXPECTED_LOG(adapter_path))
        return false;

    bool released = false;
    dbus::ObjectServer server(*connection);
    auto added = server.add_object(std::string(advertisement_path),
                                   {advertisement_interface(_advertisement, released)});
    if (SIHD_UNEXPECTED_LOG(added))
        return false;

    // bluez reads the advertisement back during the call
    auto request = bluez::method_call(*adapter_path,
                                      bluez::advertising_manager_interface,
                                      "RegisterAdvertisement",
                                      {dbus::ObjectPath(std::string(advertisement_path)), dbus::Dict {}});
    if (SIHD_UNEXPECTED_LOG(request))
        return false;
    auto registered = server.call(*request, bluez::call_timeout);
    if (!registered)
    {
        SIHD_LOG(error, "Advertiser: {}", bluez::error_of(registered.error()).message);
        return false;
    }

    {
        auto lock = _impl->waitable.guard();
        _impl->wake = [&connection] {
            (void)connection->interrupt();
        };
    }
    this->service_set_ready();

    bool advertised = true;
    dbus::Message message;
    while (!this->service_stop_requested() && !released)
    {
        auto received = message.receive(*connection, max_wait);
        if (SIHD_UNEXPECTED_LOG(received))
        {
            advertised = false;
            break;
        }
        if (*received)
        {
            auto handled = server.handle(std::move(message));
            SIHD_UNEXPECTED_LOG(handled);
        }
    }

    {
        auto lock = _impl->waitable.guard();
        _impl->wake = nullptr;
    }
    if (released)
    {
        SIHD_LOG(warning, "Advertiser: bluez released the advertisement");
        return false;
    }
    auto unregistered = bluez::call(*connection,
                                    *adapter_path,
                                    bluez::advertising_manager_interface,
                                    "UnregisterAdvertisement",
                                    {dbus::ObjectPath(std::string(advertisement_path))},
                                    bluez::call_timeout);
    SIHD_UNEXPECTED_LOG(unregistered);
    return advertised;
}

bool Advertiser::on_stop()
{
    auto lock = _impl->waitable.guard();
    if (_impl->wake)
        _impl->wake();
    return true;
}

} // namespace sihd::bt
