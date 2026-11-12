#include <algorithm>
#include <deque>
#include <mutex>

#include <fmt/format.h>

#include <sihd/bt/GattServer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#include "bluez.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;
namespace dbus = sihd::sys::linux_::dbus;

SIHD_LOGGER;

namespace
{

constexpr ZStringView application_path = "/org/sihd/bt/application";
const Duration max_wait(time::hours(1));

// the offset of a long read in the ReadValue / WriteValue options
uint16_t offset_of(dbus::Iter & options)
{
    uint16_t offset = 0;
    (void)options.for_each_entry<std::string>([&offset](const std::string & key, dbus::Iter & value) -> Result<void> {
        if (key == "offset")
            offset = value.get<uint16_t>().value_or(0);
        return {};
    });
    return offset;
}

struct Published
{
        std::string path;
        std::string service_uuid;
        LocalCharacteristic characteristic;
        std::vector<uint8_t> value;
};

} // namespace

struct GattServer::Impl
{
        // values change from notify() while the server thread reads them
        std::mutex mutex;
        std::vector<Published> locals;
        std::deque<std::string> changed;
        bool running = false;

        Waitable waitable;
        std::function<void()> wake;

        dbus::ObjectServer::Interface characteristic_interface(size_t index, const std::string & service_path)
        {
            const uint8_t properties = locals[index].characteristic.properties;
            return dbus::ObjectServer::Interface {
                .name = std::string(bluez::gatt_characteristic_interface),
                .methods =
                    {
                        dbus::ObjectServer::Method {
                            .name = "ReadValue",
                            .in = {{"options", "a{sv}"}},
                            .out = {{"value", "ay"}},
                            .handler = [this,
                                        index](dbus::ObjectServer::Call & call) { return this->read(index, call); },
                        },
                        dbus::ObjectServer::Method {
                            .name = "WriteValue",
                            .in = {{"value", "ay"}, {"options", "a{sv}"}},
                            .out = {},
                            .handler = [this,
                                        index](dbus::ObjectServer::Call & call) { return this->write(index, call); },
                        },
                        dbus::ObjectServer::Method {
                            .name = "StartNotify",
                            .in = {},
                            .out = {},
                            .handler = [](dbus::ObjectServer::Call & call) { return call.reply(); },
                        },
                        dbus::ObjectServer::Method {
                            .name = "StopNotify",
                            .in = {},
                            .out = {},
                            .handler = [](dbus::ObjectServer::Call & call) { return call.reply(); },
                        },
                    },
                .properties =
                    {
                        dbus::ObjectServer::Property::read_only<std::string>(
                            "UUID",
                            [this, index] { return locals[index].characteristic.uuid; }),
                        dbus::ObjectServer::Property {
                            .name = "Service",
                            .signature = "o",
                            .get = [service_path](dbus::Appender & to) { return to.add_object_path(service_path); },
                            .set = {},
                            .emits_changed = false,
                        },
                        dbus::ObjectServer::Property {
                            .name = "Flags",
                            .signature = "as",
                            .get =
                                [properties](dbus::Appender & to) { return to.add_array(bluez::flags_of(properties)); },
                            .set = {},
                            .emits_changed = false,
                        },
                        dbus::ObjectServer::Property {
                            .name = "Value",
                            .signature = "ay",
                            .get =
                                [this, index](dbus::Appender & to) {
                                    std::lock_guard lock(mutex);
                                    return to.add_array(locals[index].value);
                                },
                            .set = {},
                            .emits_changed = false,
                        },
                    },
                .signals = {},
            };
        }

        Result<void> read(size_t index, dbus::ObjectServer::Call & call)
        {
            dbus::Iter options(call.message());
            const uint16_t offset = offset_of(options);
            const LocalCharacteristic & characteristic = locals[index].characteristic;
            std::vector<uint8_t> value;
            if (characteristic.on_read)
                value = characteristic.on_read();
            else
            {
                std::lock_guard lock(mutex);
                value = locals[index].value;
            }

            if (offset > value.size())
            {
                auto refused = call.message().error_return("org.bluez.Error.InvalidOffset", "read past the value");
                SIHD_UNEXPECTED_RETURN(refused);
                return call.send(*refused);
            }

            auto reply = call.message().method_return();
            SIHD_UNEXPECTED_RETURN(reply);
            dbus::Appender body(*reply);
            auto added = body.add_array(std::vector<uint8_t>(value.begin() + offset, value.end()));
            SIHD_UNEXPECTED_RETURN(added);
            return call.send(*reply);
        }

        Result<void> write(size_t index, dbus::ObjectServer::Call & call)
        {
            dbus::Iter arguments(call.message());
            auto value = arguments.get_array<uint8_t>();
            SIHD_UNEXPECTED_RETURN(value);
            const LocalCharacteristic & characteristic = locals[index].characteristic;
            if (characteristic.on_write && !characteristic.on_write(*value))
            {
                auto refused = call.message().error_return(bluez::not_permitted_error, "write refused");
                SIHD_UNEXPECTED_RETURN(refused);
                return call.send(*refused);
            }
            {
                std::lock_guard lock(mutex);
                locals[index].value = std::move(*value);
            }
            return call.reply();
        }
};

GattServer::GattServer(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>())
{
    this->set_adapter(0);
    this->add_conf("adapter", &GattServer::set_adapter);
}

GattServer::~GattServer()
{
    if (this->is_running())
        this->stop();
}

Result<void>
    GattServer::notify(const std::string & service_uuid, const std::string & characteristic_uuid, ArrUByteView value)
{
    {
        std::lock_guard lock(_impl->mutex);
        if (!_impl->running)
            return std::unexpected(Error(closed, "gatt server is not started"));
        auto it = std::ranges::find_if(_impl->locals, [&](const Published & local) {
            return str::to_lower_copy(local.service_uuid) == str::to_lower_copy(service_uuid)
                   && str::to_lower_copy(local.characteristic.uuid) == str::to_lower_copy(characteristic_uuid);
        });
        if (it == _impl->locals.end())
            return std::unexpected(
                Error(not_found, "no characteristic {} in service {}", characteristic_uuid, service_uuid));
        if ((it->characteristic.properties & (GattCharacteristic::notify | GattCharacteristic::indicate)) == 0)
            return std::unexpected(
                Error(invalid_argument, "characteristic {} neither notifies nor indicates", characteristic_uuid));
        it->value.assign(value.begin(), value.end());
        _impl->changed.emplace_back(it->path);
    }
    auto lock = _impl->waitable.guard();
    if (_impl->wake)
        _impl->wake();
    return {};
}

bool GattServer::on_start()
{
    auto connection = bluez::open_system_bus();
    if (SIHD_UNEXPECTED_LOG(connection))
        return false;
    auto adapter_path = bluez::adapter_path(*connection, _adapter_index);
    if (SIHD_UNEXPECTED_LOG(adapter_path))
        return false;

    dbus::ObjectServer server(*connection);
    auto managed = server.add_object_manager(std::string(application_path));
    if (SIHD_UNEXPECTED_LOG(managed))
        return false;
    {
        std::lock_guard lock(_impl->mutex);
        _impl->locals.clear();
        _impl->changed.clear();
        for (size_t s = 0; s < _services.size(); ++s)
        {
            for (size_t c = 0; c < _services[s].characteristics.size(); ++c)
            {
                _impl->locals.push_back(Published {
                    .path = fmt::format("{}/service{}/char{}", application_path, s, c),
                    .service_uuid = _services[s].uuid,
                    .characteristic = _services[s].characteristics[c],
                    .value = {},
                });
            }
        }
    }

    size_t index = 0;
    for (size_t s = 0; s < _services.size(); ++s)
    {
        const std::string service_path = fmt::format("{}/service{}", application_path, s);
        const LocalService & service = _services[s];
        auto service_added = server.add_object(
            service_path,
            {dbus::ObjectServer::Interface {
                .name = std::string(bluez::gatt_service_interface),
                .methods = {},
                .properties =
                    {dbus::ObjectServer::Property::read_only<std::string>("UUID", [service] { return service.uuid; }),
                     dbus::ObjectServer::Property::read_only<bool>("Primary", [service] { return service.primary; })},
                .signals = {},
            }});
        if (SIHD_UNEXPECTED_LOG(service_added))
            return false;

        for (size_t c = 0; c < service.characteristics.size(); ++c, ++index)
        {
            auto added = server.add_object(_impl->locals[index].path,
                                           {_impl->characteristic_interface(index, service_path)});
            if (SIHD_UNEXPECTED_LOG(added))
                return false;
        }
    }

    // bluez reads the whole tree back during the call
    auto request = bluez::method_call(*adapter_path,
                                      bluez::gatt_manager_interface,
                                      "RegisterApplication",
                                      [](dbus::Appender & to) -> Result<void> {
                                          auto path = to.add_object_path(application_path);
                                          SIHD_UNEXPECTED_RETURN(path);
                                          return to.add_empty_vardict();
                                      });
    if (SIHD_UNEXPECTED_LOG(request))
        return false;
    auto registered = server.call(*request, bluez::call_timeout);
    if (!registered)
    {
        SIHD_LOG(error, "GattServer: {}", bluez::error_of(registered.error()).message);
        return false;
    }

    {
        std::lock_guard lock(_impl->mutex);
        _impl->running = true;
    }
    {
        auto lock = _impl->waitable.guard();
        _impl->wake = [&connection] {
            (void)connection->interrupt();
        };
    }
    this->service_set_ready();

    bool served = true;
    dbus::Message message;
    while (!this->service_stop_requested())
    {
        std::deque<std::string> changed;
        {
            std::lock_guard lock(_impl->mutex);
            changed.swap(_impl->changed);
        }
        for (const std::string & path : changed)
        {
            auto emitted = server.emit_properties_changed(path,
                                                          std::string(bluez::gatt_characteristic_interface),
                                                          {"Value"});
            SIHD_UNEXPECTED_LOG(emitted);
        }

        auto received = message.receive(*connection, max_wait);
        if (SIHD_UNEXPECTED_LOG(received))
        {
            served = false;
            break;
        }
        if (!*received)
            continue;
        auto handled = server.handle(std::move(message));
        SIHD_UNEXPECTED_LOG(handled);
    }

    {
        auto lock = _impl->waitable.guard();
        _impl->wake = nullptr;
    }
    {
        std::lock_guard lock(_impl->mutex);
        _impl->running = false;
    }
    auto unregistered = bluez::call(
        *connection,
        *adapter_path,
        bluez::gatt_manager_interface,
        "UnregisterApplication",
        [](dbus::Appender & to) { return to.add_object_path(application_path); },
        bluez::call_timeout);
    SIHD_UNEXPECTED_LOG(unregistered);
    return served;
}

bool GattServer::on_stop()
{
    auto lock = _impl->waitable.guard();
    if (_impl->wake)
        _impl->wake();
    return true;
}

} // namespace sihd::bt
