#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include <fmt/format.h>

#include <sihd/bt/Device.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
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

constexpr ZStringView agent_path = "/org/sihd/bt/agent";
// the loop wakes up on a signal or a stop
const Duration max_wait(time::hours(1));

struct AttributePaths
{
        std::map<uint16_t, std::string> characteristics;
        std::map<uint16_t, std::string> descriptors;
};

struct PairingAgent
{
        PairingHandler handler;
        std::string address;

        PairingAnswer ask(PairingRequest::Kind kind, uint32_t passkey, std::string pin) const
        {
            return handler(
                PairingRequest {.address = address, .kind = kind, .passkey = passkey, .pin = std::move(pin)});
        }
};

// one thread per AcquireNotify socket: each datagram is one notification
class NotifySocket
{
    public:
        using Handler = std::function<void(std::vector<uint8_t> &&)>;

        NotifySocket(int fd, uint16_t mtu, Handler handler): _fd(fd)
        {
            _thread = std::jthread([this, mtu, handler = std::move(handler)] {
                std::vector<uint8_t> buffer(std::max<uint16_t>(mtu, 23));
                while (true)
                {
                    const ssize_t received = ::recv(_fd, buffer.data(), buffer.size(), 0);
                    if (received <= 0)
                        return;
                    handler(std::vector<uint8_t>(buffer.begin(), buffer.begin() + received));
                }
            });
        }

        ~NotifySocket()
        {
            ::shutdown(_fd, SHUT_RDWR);
            _thread.join();
            ::close(_fd);
        }

        NotifySocket(const NotifySocket &) = delete;
        NotifySocket & operator=(const NotifySocket &) = delete;

    private:
        int _fd;
        std::jthread _thread;
};

std::string device_rule(const std::string & device_path)
{
    return fmt::format("type='signal',sender='{}',interface='{}',member='PropertiesChanged',path_namespace='{}'",
                       bluez::service,
                       bluez::properties_interface,
                       device_path);
}

Error not_connected()
{
    return Error(closed, "bluetooth device is not connected");
}

Result<std::vector<uint8_t>> read_value_reply(const dbus::Message & reply)
{
    return dbus::Iter(reply).get_array<uint8_t>();
}

bluez::Arguments write_arguments(ArrUByteView value, std::optional<WriteType> type)
{
    return [value, type](dbus::Appender & to) -> Result<void> {
        auto bytes = to.add_array(std::vector<uint8_t>(value.begin(), value.end()));
        SIHD_UNEXPECTED_RETURN(bytes);
        auto options = to.open_array("{sv}");
        SIHD_UNEXPECTED_RETURN(options);
        if (type.has_value())
        {
            const std::string kind = *type == WriteType::with_response ? "request" : "command";
            auto added = options->add_entry("type", "s", [&kind](dbus::Appender & entry) { return entry.add(kind); });
            SIHD_UNEXPECTED_RETURN(added);
        }
        return to.close(std::move(*options));
    };
}

Result<std::string> path_in(const std::map<uint16_t, std::string> & known, uint16_t handle)
{
    if (auto it = known.find(handle); it != known.end())
        return it->second;
    return std::unexpected(Error(not_found, "no gatt attribute of handle {}, services() lists them", handle));
}

std::vector<GattService>
    build_services(const bluez::ManagedObjects & objects, const std::string & device_path, AttributePaths & paths)
{
    std::vector<GattService> services;
    paths = {};
    for (const auto & [service_path, service] : objects.gatt)
    {
        if (service.interface != bluez::gatt_service_interface || service.parent != device_path)
            continue;
        GattService & built = services.emplace_back(GattService {.uuid = service.uuid,
                                                                 .handle = service.handle,
                                                                 .primary = service.primary,
                                                                 .characteristics = {}});
        for (const auto & [characteristic_path, characteristic] : objects.gatt)
        {
            if (characteristic.interface != bluez::gatt_characteristic_interface
                || characteristic.parent != service_path)
                continue;
            paths.characteristics.insert_or_assign(characteristic.handle, characteristic_path);
            GattCharacteristic & built_characteristic = built.characteristics.emplace_back(GattCharacteristic {
                .uuid = characteristic.uuid,
                .handle = characteristic.handle,
                .properties = characteristic.properties,
                .descriptors = {},
            });

            for (const auto & [descriptor_path, descriptor] : objects.gatt)
            {
                if (descriptor.interface != bluez::gatt_descriptor_interface
                    || descriptor.parent != characteristic_path)
                    continue;
                paths.descriptors.insert_or_assign(descriptor.handle, descriptor_path);
                built_characteristic.descriptors.emplace_back(
                    GattDescriptor {.uuid = descriptor.uuid, .handle = descriptor.handle});
            }

            std::ranges::sort(built_characteristic.descriptors, {}, &GattDescriptor::handle);
        }
        std::ranges::sort(built.characteristics, {}, &GattCharacteristic::handle);
    }
    std::ranges::sort(services, {}, &GattService::handle);
    return services;
}

// the passkey or the pin code following the device path
template <dbus::BasicType T>
Result<T> second_argument(dbus::ObjectServer::Call & call)
{
    dbus::Iter arguments(call.message());
    arguments.next();
    return arguments.get<T>();
}

Result<void> reply_or_reject(dbus::ObjectServer::Call & call, bool accepted)
{
    if (accepted)
        return call.reply();
    auto rejection = call.message().error_return(bluez::rejected_error, "pairing rejected");
    SIHD_UNEXPECTED_RETURN(rejection);
    return call.send(*rejection);
}

dbus::ObjectServer::Method agent_method(const std::string & name,
                                        std::vector<dbus::ObjectServer::Arg> in,
                                        std::vector<dbus::ObjectServer::Arg> out,
                                        dbus::ObjectServer::MethodHandler handler)
{
    return dbus::ObjectServer::Method {
        .name = name,
        .in = std::move(in),
        .out = std::move(out),
        .handler = std::move(handler),
    };
}

// Agent1 forwards each question to the handler, Display* only tells it
dbus::ObjectServer::Interface agent_interface(const PairingAgent & agent)
{
    return dbus::ObjectServer::Interface {
        .name = std::string(bluez::agent_interface),
        .methods =
            {
                agent_method("Release", {}, {}, [](dbus::ObjectServer::Call & call) { return call.reply(); }),
                agent_method("Cancel", {}, {}, [](dbus::ObjectServer::Call & call) { return call.reply(); }),
                agent_method("RequestPinCode",
                             {{"device", "o"}},
                             {{"pincode", "s"}},
                             [agent](dbus::ObjectServer::Call & call) -> Result<void> {
                                 const PairingAnswer answer = agent.ask(PairingRequest::Kind::enter_pin, 0, "");
                                 if (!answer.accept)
                                     return reply_or_reject(call, false);
                                 return call.reply(answer.pin);
                             }),
                agent_method("DisplayPinCode",
                             {{"device", "o"}, {"pincode", "s"}},
                             {},
                             [agent](dbus::ObjectServer::Call & call) -> Result<void> {
                                 auto pin = second_argument<std::string>(call);
                                 SIHD_UNEXPECTED_RETURN(pin);
                                 agent.ask(PairingRequest::Kind::display_pin, 0, std::move(*pin));
                                 return call.reply();
                             }),

                agent_method("RequestPasskey",
                             {{"device", "o"}},
                             {{"passkey", "u"}},
                             [agent](dbus::ObjectServer::Call & call) -> Result<void> {
                                 const PairingAnswer answer = agent.ask(PairingRequest::Kind::enter_passkey, 0, "");
                                 if (!answer.accept)
                                     return reply_or_reject(call, false);
                                 return call.reply(answer.passkey);
                             }),
                agent_method("DisplayPasskey",
                             {{"device", "o"}, {"passkey", "u"}, {"entered", "q"}},
                             {},
                             [agent](dbus::ObjectServer::Call & call) -> Result<void> {
                                 auto passkey = second_argument<uint32_t>(call);
                                 SIHD_UNEXPECTED_RETURN(passkey);
                                 agent.ask(PairingRequest::Kind::display_passkey, *passkey, "");
                                 return call.reply();
                             }),

                agent_method(
                    "RequestConfirmation",
                    {{"device", "o"}, {"passkey", "u"}},
                    {},
                    [agent](dbus::ObjectServer::Call & call) -> Result<void> {
                        auto passkey = second_argument<uint32_t>(call);
                        SIHD_UNEXPECTED_RETURN(passkey);
                        const PairingAnswer answer = agent.ask(PairingRequest::Kind::confirm_passkey, *passkey, "");
                        return reply_or_reject(call, answer.accept);
                    }),
                agent_method("RequestAuthorization",
                             {{"device", "o"}},
                             {},
                             [agent](dbus::ObjectServer::Call & call) {
                                 return reply_or_reject(call, agent.ask(PairingRequest::Kind::confirm, 0, "").accept);
                             }),
                agent_method("AuthorizeService",
                             {{"device", "o"}, {"uuid", "s"}},
                             {},
                             [agent](dbus::ObjectServer::Call & call) {
                                 return reply_or_reject(call, agent.ask(PairingRequest::Kind::confirm, 0, "").accept);
                             }),
            },
        .properties = {},
        .signals = {},
    };
}

Result<void> register_agent(dbus::Connection & connection)
{
    auto registered = bluez::call(
        connection,
        bluez::manager_path,
        bluez::agent_manager_interface,
        "RegisterAgent",
        [](dbus::Appender & to) -> Result<void> {
            auto path = to.add_object_path(agent_path);
            SIHD_UNEXPECTED_RETURN(path);
            return to.add("KeyboardDisplay");
        },
        bluez::call_timeout);
    SIHD_UNEXPECTED_RETURN(registered);
    return {};
}

void unregister_agent(dbus::Connection & connection)
{
    // the connection leaving the bus unregisters it as well
    (void)bluez::call(
        connection,
        bluez::manager_path,
        bluez::agent_manager_interface,
        "UnregisterAgent",
        [](dbus::Appender & to) { return to.add_object_path(agent_path); },
        bluez::call_timeout);
}

} // namespace

struct Device::Impl
{
        // consumer threads call through this one, the start() thread receives on its own
        std::mutex calls_mutex;
        dbus::Connection calls;
        std::string device_path;
        std::string adapter_path;
        // from the last services()
        AttributePaths paths;

        Waitable waitable;
        bool running = false;
        bool services_resolved = false;
        std::function<void()> wake;

        std::mutex subscriptions_mutex;
        std::map<uint16_t, std::unique_ptr<NotifySocket>> acquired;
        // characteristics notifying through their Value property: path -> characteristic
        std::map<std::string, GattCharacteristic> started;

        Result<std::string> characteristic_path(uint16_t handle)
        {
            std::lock_guard lock(calls_mutex);
            return path_in(paths.characteristics, handle);
        }

        Result<std::string> descriptor_path(uint16_t handle)
        {
            std::lock_guard lock(calls_mutex);
            return path_in(paths.descriptors, handle);
        }

        // the device path once calls is open and events subscribed to the device signals
        Result<std::string> open(dbus::Connection & events, const std::string & address, size_t adapter_index);
        // until a stop or a lost link
        void follow(Device & device, dbus::Connection & events);
        // false once the link is lost
        bool follow_device(const dbus::Message & signal);
        std::optional<GattNotification> notification_of(const dbus::Message & signal);
        void close(dbus::Connection & events, bool disconnect);
};

Result<std::string> Device::Impl::open(dbus::Connection & events, const std::string & address, size_t adapter_index)
{
    auto objects = bluez::managed_objects(events);
    SIHD_UNEXPECTED_RETURN(objects);
    auto adapter = bluez::adapter_path(*objects, adapter_index);
    SIHD_UNEXPECTED_RETURN(adapter);
    const std::string path = bluez::device_path(*adapter, address);
    const auto device = objects->devices.find(path);
    if (device == objects->devices.end())
        return std::unexpected(Error(not_found, "bluez does not know {}, a scan finds it", address));

    auto matched = events.add_match(device_rule(path));
    SIHD_UNEXPECTED_RETURN(matched);
    {
        std::lock_guard lock(calls_mutex);
        auto calls_opened = calls.open(dbus::Bus::system);
        SIHD_UNEXPECTED_RETURN(calls_opened);
        device_path = path;
        adapter_path = *adapter;
        paths = {};
    }
    auto lock = waitable.guard();
    wake = [&events] {
        (void)events.interrupt();
    };
    services_resolved = device->second.services_resolved;
    return path;
}

void Device::Impl::follow(Device & device, dbus::Connection & events)
{
    dbus::Message signal;
    while (!device.service_stop_requested())
    {
        device._flush_notifications();
        auto received = signal.receive(events, max_wait);
        if (SIHD_UNEXPECTED_LOG(received))
            return;
        if (!*received)
            continue;

        if (signal.path() == device_path)
        {
            if (!this->follow_device(signal))
                return;
        }
        else if (auto notification = this->notification_of(signal))
            device._queue_notification(std::move(*notification));
    }
}

bool Device::Impl::follow_device(const dbus::Message & signal)
{
    auto resolved = signal.changed_property(bluez::device_interface, "ServicesResolved").value_or(std::nullopt);
    if (resolved.has_value())
    {
        {
            auto lock = waitable.guard();
            services_resolved = resolved->get<bool>().value_or(false);
        }
        waitable.notify_all();
    }
    auto connected = signal.changed_property(bluez::device_interface, "Connected").value_or(std::nullopt);
    return !connected.has_value() || connected->get<bool>().value_or(true);
}

// the Value change of a characteristic subscribed through StartNotify
std::optional<GattNotification> Device::Impl::notification_of(const dbus::Message & signal)
{
    auto value = signal.changed_property(bluez::gatt_characteristic_interface, "Value").value_or(std::nullopt);
    if (!value.has_value())
        return std::nullopt;
    auto bytes = value->get_array<uint8_t>();
    if (SIHD_UNEXPECTED_LOG(bytes))
        return std::nullopt;

    std::lock_guard subscriptions(subscriptions_mutex);
    const auto it = started.find(signal.path());
    if (it == started.end())
        return std::nullopt;
    return GattNotification {
        .handle = it->second.handle,
        .uuid = it->second.uuid,
        .value = std::move(*bytes),
        .timestamp = {},
    };
}

void Device::Impl::close(dbus::Connection & events, bool disconnect)
{
    {
        auto lock = waitable.guard();
        wake = nullptr;
        running = false;
    }
    waitable.notify_all();
    {
        std::lock_guard subscriptions(subscriptions_mutex);
        acquired.clear();
        started.clear();
    }

    if (disconnect)
    {
        auto disconnected = bluez::call(events,
                                        device_path,
                                        bluez::device_interface,
                                        "Disconnect",
                                        {},
                                        bluez::call_timeout);
        SIHD_UNEXPECTED_LOG(disconnected);
    }
    std::lock_guard lock(calls_mutex);
    calls.close();
    paths = {};
}

Device::Device(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>())
{
    this->_add_confs();
}

Device::~Device()
{
    if (this->is_running())
        this->stop();
}

Result<DeviceInfo> Device::info() const
{
    auto objects = bluez::open_managed_objects();
    SIHD_UNEXPECTED_RETURN(objects);
    auto adapter_path = bluez::adapter_path(*objects, _adapter_index);
    SIHD_UNEXPECTED_RETURN(adapter_path);
    const auto it = objects->devices.find(bluez::device_path(*adapter_path, _address));
    if (it == objects->devices.end())
        return std::unexpected(Error(not_found, "bluez does not know {}", _address));
    return bluez::device_info(it->second);
}

Result<void> Device::pair()
{
    auto connection = bluez::open_system_bus();
    SIHD_UNEXPECTED_RETURN(connection);
    auto adapter_path = bluez::adapter_path(*connection, _adapter_index);
    SIHD_UNEXPECTED_RETURN(adapter_path);

    // bluez asks the agent of the connection calling Pair, the default agent without one
    dbus::ObjectServer server(*connection);
    const bool own_agent = static_cast<bool>(_pairing_handler);
    if (own_agent)
    {
        auto added = server.add_object(
            std::string(agent_path),
            {agent_interface(PairingAgent {.handler = _pairing_handler, .address = _address})});
        SIHD_UNEXPECTED_RETURN(added);
        auto registered = register_agent(*connection);
        SIHD_UNEXPECTED_RETURN(registered);
    }

    auto request = bluez::method_call(bluez::device_path(*adapter_path, _address), bluez::device_interface, "Pair", {});
    SIHD_UNEXPECTED_RETURN(request);
    auto paired = server.call(*request, bluez::pair_timeout);
    if (own_agent)
        unregister_agent(*connection);
    if (!paired)
        return std::unexpected(bluez::error_of(paired.error()));
    return {};
}

Result<void> Device::cancel_pairing()
{
    auto connection = bluez::open_system_bus();
    SIHD_UNEXPECTED_RETURN(connection);
    auto adapter_path = bluez::adapter_path(*connection, _adapter_index);
    SIHD_UNEXPECTED_RETURN(adapter_path);
    auto canceled = bluez::call(*connection,
                                bluez::device_path(*adapter_path, _address),
                                bluez::device_interface,
                                "CancelPairing",
                                {},
                                bluez::call_timeout);
    SIHD_UNEXPECTED_RETURN(canceled);
    return {};
}

Result<void> Device::unpair()
{
    auto connection = bluez::open_system_bus();
    SIHD_UNEXPECTED_RETURN(connection);
    auto adapter_path = bluez::adapter_path(*connection, _adapter_index);
    SIHD_UNEXPECTED_RETURN(adapter_path);
    const std::string device_path = bluez::device_path(*adapter_path, _address);
    auto removed = bluez::call(
        *connection,
        *adapter_path,
        bluez::adapter_interface,
        "RemoveDevice",
        [&device_path](dbus::Appender & to) { return to.add_object_path(device_path); },
        bluez::call_timeout);
    SIHD_UNEXPECTED_RETURN(removed);
    return {};
}

Result<void> Device::set_trusted(bool trusted)
{
    auto connection = bluez::open_system_bus();
    SIHD_UNEXPECTED_RETURN(connection);
    auto adapter_path = bluez::adapter_path(*connection, _adapter_index);
    SIHD_UNEXPECTED_RETURN(adapter_path);
    return bluez::set_property(*connection,
                               bluez::device_path(*adapter_path, _address),
                               bluez::device_interface,
                               "Trusted",
                               trusted);
}

Result<std::vector<GattService>> Device::services()
{
    _impl->waitable.wait_for(bluez::connect_timeout, [this] { return !_impl->running || _impl->services_resolved; });
    {
        auto guard = _impl->waitable.guard();
        if (!_impl->running)
            return std::unexpected(not_connected());
        if (!_impl->services_resolved)
            return std::unexpected(Error(timeout, "the services of {} are not resolved", _address));
    }
    std::lock_guard lock(_impl->calls_mutex);
    auto objects = bluez::managed_objects(_impl->calls);
    SIHD_UNEXPECTED_RETURN(objects);
    return build_services(*objects, _impl->device_path, _impl->paths);
}

Result<std::vector<uint8_t>> Device::read_characteristic(const GattCharacteristic & characteristic)
{
    auto path = _impl->characteristic_path(characteristic.handle);
    SIHD_UNEXPECTED_RETURN(path);
    std::lock_guard lock(_impl->calls_mutex);
    auto called = bluez::call(_impl->calls,
                              *path,
                              bluez::gatt_characteristic_interface,
                              "ReadValue",
                              &dbus::Appender::add_empty_vardict,
                              bluez::connect_timeout);
    SIHD_UNEXPECTED_RETURN(called);
    return read_value_reply(*called);
}

Result<void> Device::write_characteristic(const GattCharacteristic & characteristic, ArrUByteView value, WriteType type)
{
    auto path = _impl->characteristic_path(characteristic.handle);
    SIHD_UNEXPECTED_RETURN(path);
    std::lock_guard lock(_impl->calls_mutex);
    auto called = bluez::call(_impl->calls,
                              *path,
                              bluez::gatt_characteristic_interface,
                              "WriteValue",
                              write_arguments(value, type),
                              bluez::connect_timeout);
    SIHD_UNEXPECTED_RETURN(called);
    return {};
}

Result<std::vector<uint8_t>> Device::read_descriptor(const GattDescriptor & descriptor)
{
    auto path = _impl->descriptor_path(descriptor.handle);
    SIHD_UNEXPECTED_RETURN(path);
    std::lock_guard lock(_impl->calls_mutex);
    auto called = bluez::call(_impl->calls,
                              *path,
                              bluez::gatt_descriptor_interface,
                              "ReadValue",
                              &dbus::Appender::add_empty_vardict,
                              bluez::connect_timeout);
    SIHD_UNEXPECTED_RETURN(called);
    return read_value_reply(*called);
}

Result<void> Device::write_descriptor(const GattDescriptor & descriptor, ArrUByteView value)
{
    auto path = _impl->descriptor_path(descriptor.handle);
    SIHD_UNEXPECTED_RETURN(path);
    std::lock_guard lock(_impl->calls_mutex);
    auto called = bluez::call(_impl->calls,
                              *path,
                              bluez::gatt_descriptor_interface,
                              "WriteValue",
                              write_arguments(value, std::nullopt),
                              bluez::connect_timeout);
    SIHD_UNEXPECTED_RETURN(called);
    return {};
}

Result<void> Device::subscribe(const GattCharacteristic & characteristic)
{
    auto path = _impl->characteristic_path(characteristic.handle);
    SIHD_UNEXPECTED_RETURN(path);

    // AcquireNotify only serves notify: an indication goes through StartNotify and the Value property
    if ((characteristic.properties & GattCharacteristic::notify) != 0)
    {
        std::lock_guard lock(_impl->calls_mutex);
        auto called = bluez::call(_impl->calls,
                                  *path,
                                  bluez::gatt_characteristic_interface,
                                  "AcquireNotify",
                                  &dbus::Appender::add_empty_vardict,
                                  bluez::call_timeout);
        SIHD_UNEXPECTED_RETURN(called);
        dbus::Iter body(*called);
        auto fd = body.get_unix_fd();
        SIHD_UNEXPECTED_RETURN(fd);
        body.next();
        auto mtu = body.get<uint16_t>();
        SIHD_UNEXPECTED_RETURN(mtu);

        // bluez hands over a non-blocking socket, the reader thread blocks on it: the copy shares the flag
        auto blocking = sys::os::set_nonblocking(fd->fd(), false);
        SIHD_UNEXPECTED_RETURN(blocking);
        auto owned = sys::os::dup(fd->fd());
        SIHD_UNEXPECTED_RETURN(owned);

        const uint16_t handle = characteristic.handle;
        const std::string uuid = characteristic.uuid;
        auto socket = std::make_unique<NotifySocket>(*owned, *mtu, [this, handle, uuid](std::vector<uint8_t> && value) {
            this->_queue_notification(
                GattNotification {.handle = handle, .uuid = uuid, .value = std::move(value), .timestamp = {}});
        });
        std::lock_guard subscriptions(_impl->subscriptions_mutex);
        _impl->acquired.insert_or_assign(handle, std::move(socket));
        return {};
    }

    std::lock_guard lock(_impl->calls_mutex);
    {
        std::lock_guard subscriptions(_impl->subscriptions_mutex);
        _impl->started.insert_or_assign(*path, characteristic);
    }
    auto called = bluez::call(_impl->calls,
                              *path,
                              bluez::gatt_characteristic_interface,
                              "StartNotify",
                              {},
                              bluez::call_timeout);
    if (!called)
    {
        std::lock_guard subscriptions(_impl->subscriptions_mutex);
        _impl->started.erase(*path);
    }
    SIHD_UNEXPECTED_RETURN(called);
    return {};
}

Result<void> Device::unsubscribe(const GattCharacteristic & characteristic)
{
    auto path = _impl->characteristic_path(characteristic.handle);
    SIHD_UNEXPECTED_RETURN(path);
    std::unique_ptr<NotifySocket> released;
    bool was_started = false;
    {
        std::lock_guard subscriptions(_impl->subscriptions_mutex);
        if (auto it = _impl->acquired.find(characteristic.handle); it != _impl->acquired.end())
        {
            released = std::move(it->second);
            _impl->acquired.erase(it);
        }
        was_started = _impl->started.erase(*path) > 0;
    }

    // closing the acquired socket ends the notifications
    released.reset();
    if (!was_started)
        return {};

    std::lock_guard lock(_impl->calls_mutex);
    auto called = bluez::call(_impl->calls,
                              *path,
                              bluez::gatt_characteristic_interface,
                              "StopNotify",
                              {},
                              bluez::call_timeout);
    SIHD_UNEXPECTED_RETURN(called);
    return {};
}

Result<uint16_t> Device::mtu()
{
    bool listed = false;
    {
        std::lock_guard lock(_impl->calls_mutex);
        listed = !_impl->paths.characteristics.empty();
    }
    if (!listed)
    {
        auto services = this->services();
        SIHD_UNEXPECTED_RETURN(services);
    }
    std::lock_guard lock(_impl->calls_mutex);
    if (_impl->paths.characteristics.empty())
        return std::unexpected(Error(not_found, "{} has no gatt characteristic to read the mtu of", _address));
    return dbus::get_property<uint16_t>(_impl->calls,
                                        bluez::service,
                                        _impl->paths.characteristics.begin()->second,
                                        bluez::gatt_characteristic_interface,
                                        "MTU",
                                        bluez::call_timeout)
        .transform_error(bluez::error_of);
}

bool Device::on_start()
{
    auto events = bluez::open_system_bus();
    if (SIHD_UNEXPECTED_LOG(events))
        return false;
    auto device_path = _impl->open(*events, _address, _adapter_index);
    if (SIHD_UNEXPECTED_LOG(device_path))
        return false;

    // through an object server: a stop interrupts the wait
    dbus::ObjectServer server(*events);
    auto connected = bluez::method_call(*device_path, bluez::device_interface, "Connect", {})
                         .and_then([&server](const dbus::Message & request) {
                             return server.call(request, bluez::connect_timeout);
                         });

    if (connected)
    {
        // the call dropped the signals it received meanwhile: ServicesResolved may already be set
        auto resolved = dbus::get_property<bool>(*events,
                                                 bluez::service,
                                                 *device_path,
                                                 bluez::device_interface,
                                                 "ServicesResolved",
                                                 bluez::call_timeout);
        {
            auto lock = _impl->waitable.guard();
            _impl->running = true;
            _impl->services_resolved = resolved.value_or(false);
        }
        this->_publish_event(Event::connected);
        this->service_set_ready();
        _impl->follow(*this, *events);
    }
    else
        SIHD_LOG(error, "Device: cannot connect {}: {}", _address, bluez::error_of(connected.error()).message);

    // a stopped connection attempt is canceled as well
    const bool stopped = this->service_stop_requested();
    _impl->close(*events, connected || stopped);
    {
        std::lock_guard lock(_pending_mutex);
        _pending.clear();
    }
    if (connected)
        this->_publish_event(Event::disconnected);
    return connected && stopped;
}

void Device::_wake_loop()
{
    auto lock = _impl->waitable.guard();
    if (_impl->wake)
        _impl->wake();
}

bool Device::on_stop()
{
    // the service flag is already set: waking under the guard cannot slip between a check and a wait
    this->_wake_loop();
    _impl->waitable.notify_all();
    return true;
}

} // namespace sihd::bt
