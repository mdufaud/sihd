#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/bt/Device.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Synchronizer.hpp>
#include <sihd/util/time.hpp>

#include "fake_bluez.hpp"

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;

class TestBtDevice: public ::testing::Test
{
    protected:
        TestBtDevice(): _device("device") { LoggerManager::stream(); }

        virtual ~TestBtDevice() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
            ASSERT_TRUE(_daemon.start({bluez_service}));
            serve_adapter(_daemon, adapter_path, true);
            serve_device(_daemon);
            serve_gatt(_daemon);
            _daemon.on(device_interface, "Connect", [this](const dbus::Message & call) {
                this->set_connected(true);
                _daemon.reply(call, {});
            });
            _daemon.on(device_interface, "Disconnect", [this](const dbus::Message & call) {
                this->set_connected(false);
                _daemon.reply(call, {});
            });
            ASSERT_TRUE(_device.set_address(device_address));
            _device.add_observer(&_observer);
        }

        virtual void TearDown()
        {
            if (_running.joinable())
            {
                _device.stop();
                _running.join();
            }
        }

        // from a handler
        void set_connected(bool connected)
        {
            _daemon.set_property(device_path, device_interface, "Connected", boolean(connected));
            _daemon.set_property(device_path, device_interface, "ServicesResolved", boolean(connected));
            _daemon.emit_properties_changed(
                device_path,
                device_interface,
                {{"Connected", boolean(connected)}, {"ServicesResolved", boolean(connected)}});
        }

        void connect()
        {
            _running = std::jthread([this] { _started = _device.start(); });
            ASSERT_TRUE(_device.wait_ready(Duration(time::sec(5))));
        }

        void connect_and_list()
        {
            ASSERT_NO_FATAL_FAILURE(this->connect());
            auto services = _device.services();
            ASSERT_TRUE(services.has_value()) << services.error().message;
        }

        void disconnect()
        {
            _device.stop();
            _running.join();
        }

        // the barrier an observer reaches on the first notification
        void wait_notification(Synchronizer & notified, Handler<Device *> & waiter)
        {
            notified.init_sync(2);
            waiter.set_method([&notified](Device *device) {
                if (device->last_event() == Device::Event::notification)
                    notified.sync(Duration(time::sec(5)));
            });
            _device.add_observer(&waiter);
        }

        void serve_pairing(bool confirmed_by_agent)
        {
            _daemon.on(agent_manager_interface, "RegisterAgent", [this](const dbus::Message & call) {
                _agent_sender = call.sender();
                _agent_path = dbus::Iter(call).get<std::string>().value_or("");
                _daemon.reply(call, {});
            });
            answer(_daemon, agent_manager_interface, {"UnregisterAgent"});

            _daemon.on(device_interface, "Pair", [this, confirmed_by_agent](const dbus::Message & call) {
                if (!confirmed_by_agent)
                    return _daemon.reply(call, {});
                dbus::Message request;
                auto built = request.method_call(_agent_sender, _agent_path, agent_interface, "RequestConfirmation");
                ASSERT_TRUE(built.has_value()) << built.error().message;
                ASSERT_TRUE(request.add_object_path(device_path).has_value());
                ASSERT_TRUE(request.add(uint32_t {123456}).has_value());
                if (!request.call(_daemon.connection(), fake_timeout))
                    return _daemon.fail(call, "org.bluez.Error.AuthenticationRejected", "Authentication Rejected");
                _daemon.reply(call, {});
            });
        }

        const GattCharacteristic level {
            .uuid = level_uuid,
            .handle = 0x000d,
            .properties = GattCharacteristic::read | GattCharacteristic::notify,
            .descriptors = {},
        };
        const GattCharacteristic alert {.uuid = alert_uuid,
                                        .handle = 0x0010,
                                        .properties = GattCharacteristic::indicate,
                                        .descriptors = {}};
        const GattDescriptor cccd {.uuid = cccd_uuid, .handle = 0x000f};

        FakeDaemon _daemon;
        std::string _agent_sender;
        std::string _agent_path;
        std::vector<Device::Event> _events;
        Handler<Device *> _observer {[this](Device *device) { _events.push_back(device->last_event()); }};
        Device _device;
        bool _started = false;
        std::jthread _running;
};

TEST_F(TestBtDevice, test_connect_and_disconnect)
{
    _daemon.serve();

    ASSERT_NO_FATAL_FAILURE(this->connect());
    const bool running = _device.is_running();
    this->disconnect();
    _daemon.stop();

    EXPECT_TRUE(running);
    EXPECT_TRUE(_started);
    EXPECT_EQ(_events, (std::vector<Device::Event> {Device::Event::connected, Device::Event::disconnected}));
    EXPECT_TRUE(_daemon.called(device_interface, "Connect"));
    EXPECT_TRUE(_daemon.called(device_interface, "Disconnect"));
}

TEST_F(TestBtDevice, test_unknown_device)
{
    _daemon.serve();
    ASSERT_TRUE(_device.set_address("11:22:33:44:55:66"));

    const bool started = _device.start();
    _daemon.stop();

    EXPECT_FALSE(started);
    EXPECT_TRUE(_events.empty());
    EXPECT_FALSE(_daemon.called(device_interface, "Connect"));
}

TEST_F(TestBtDevice, test_connection_refused)
{
    _daemon.on(device_interface, "Connect", [this](const dbus::Message & call) {
        _daemon.fail(call, "org.bluez.Error.Failed", "le-connection-abort-by-local");
    });
    _daemon.serve();

    const bool started = _device.start();
    _daemon.stop();

    EXPECT_FALSE(started);
    EXPECT_TRUE(_events.empty());
    EXPECT_FALSE(_daemon.called(device_interface, "Disconnect"));
}

TEST_F(TestBtDevice, test_remote_disconnection)
{
    _daemon.on(gatt_characteristic_interface, "ReadValue", [this](const dbus::Message & call) {
        this->set_connected(false);
        _daemon.fail(call, "org.bluez.Error.NotConnected", "Not Connected");
    });
    _daemon.serve();
    ASSERT_NO_FATAL_FAILURE(this->connect_and_list());

    auto read = _device.read_characteristic(level);
    _running.join();

    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, ErrorCode::closed);
    EXPECT_FALSE(_started);
    EXPECT_EQ(_events, (std::vector<Device::Event> {Device::Event::connected, Device::Event::disconnected}));
}

TEST_F(TestBtDevice, test_info)
{
    _daemon.serve();
    ASSERT_TRUE(_device.set_address("aa:bb:cc:dd:ee:ff"));

    auto info = _device.info();

    ASSERT_TRUE(info.has_value()) << info.error().message;
    EXPECT_EQ(info->address, device_address);
    EXPECT_EQ(info->address_type, AddressType::random);
    EXPECT_EQ(info->name, "thermometer");
    EXPECT_FALSE(info->paired);
    EXPECT_FALSE(info->connected);
    EXPECT_FALSE(info->trusted);
    EXPECT_EQ(info->appearance, 0x0300);
    EXPECT_FALSE(info->class_of_device.has_value());
    EXPECT_EQ(info->uuids, std::vector<std::string> {battery_uuid});
}

TEST_F(TestBtDevice, test_invalid_address)
{
    EXPECT_FALSE(_device.set_address("AA:BB:CC:DD:EE"));
    EXPECT_FALSE(_device.set_address("AA-BB-CC-DD-EE-FF"));
    EXPECT_FALSE(_device.set_address("GG:BB:CC:DD:EE:FF"));
}

TEST_F(TestBtDevice, test_services)
{
    _daemon.serve();
    ASSERT_NO_FATAL_FAILURE(this->connect());

    auto services = _device.services();
    this->disconnect();

    ASSERT_TRUE(services.has_value()) << services.error().message;
    ASSERT_EQ(services->size(), 1u);
    const GattService & battery = services->at(0);
    EXPECT_EQ(battery.uuid, battery_uuid);
    EXPECT_EQ(battery.handle, 0x000c);
    EXPECT_TRUE(battery.primary);
    ASSERT_EQ(battery.characteristics.size(), 2u);

    const GattCharacteristic & listed_level = battery.characteristics[0];
    EXPECT_EQ(listed_level.uuid, level_uuid);
    EXPECT_EQ(listed_level.handle, 0x000d);
    EXPECT_EQ(listed_level.properties, GattCharacteristic::read | GattCharacteristic::notify);
    ASSERT_EQ(listed_level.descriptors.size(), 1u);
    EXPECT_EQ(listed_level.descriptors[0].uuid, cccd_uuid);
    EXPECT_EQ(listed_level.descriptors[0].handle, 0x000f);

    const GattCharacteristic & listed_alert = battery.characteristics[1];
    EXPECT_EQ(listed_alert.handle, 0x0010);
    EXPECT_EQ(listed_alert.properties, GattCharacteristic::indicate);
    EXPECT_TRUE(listed_alert.descriptors.empty());
}

TEST_F(TestBtDevice, test_services_need_connection)
{
    _daemon.serve();

    auto services = _device.services();

    ASSERT_FALSE(services.has_value());
    EXPECT_EQ(services.error().code, ErrorCode::closed);
    EXPECT_EQ(services.error().message, "bluetooth device is not connected");
}

TEST_F(TestBtDevice, test_attribute_needs_services)
{
    _daemon.serve();
    ASSERT_NO_FATAL_FAILURE(this->connect());

    auto read = _device.read_characteristic(level);
    this->disconnect();

    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, ErrorCode::not_found);
    EXPECT_EQ(read.error().message, "no gatt attribute of handle 13, services() lists them");
}

TEST_F(TestBtDevice, test_read_write_characteristic)
{
    std::vector<uint8_t> written;
    std::string write_type;
    _daemon.on(gatt_characteristic_interface, "ReadValue", [this](const dbus::Message & call) {
        _daemon.reply(call, {bytes(std::vector<uint8_t> {0x63})});
    });

    _daemon.on(gatt_characteristic_interface, "WriteValue", [&](const dbus::Message & call) {
        dbus::Iter arguments(call);
        written = arguments.get_array<uint8_t>().value_or(std::vector<uint8_t> {});
        arguments.next();
        (void)arguments.for_each_entry<std::string>([&write_type](const std::string & key, dbus::Iter & value) {
            if (key == "type")
                write_type = value.get<std::string>().value_or("");
            return Result<void> {};
        });
        _daemon.reply(call, {});
    });

    _daemon.serve();
    ASSERT_NO_FATAL_FAILURE(this->connect_and_list());
    const std::vector<uint8_t> payload {0x01, 0x02};

    auto read = _device.read_characteristic(level);
    auto write = _device.write_characteristic(level, payload, WriteType::without_response);
    this->disconnect();
    _daemon.stop();

    ASSERT_TRUE(read.has_value()) << read.error().message;
    EXPECT_EQ(*read, std::vector<uint8_t> {0x63});
    EXPECT_TRUE(write.has_value()) << write.error().message;
    EXPECT_EQ(written, payload);
    EXPECT_EQ(write_type, "command");
}

TEST_F(TestBtDevice, test_read_write_descriptor)
{
    std::vector<uint8_t> written;
    _daemon.on(gatt_descriptor_interface, "ReadValue", [this](const dbus::Message & call) {
        _daemon.reply(call, {bytes(std::vector<uint8_t> {0x00, 0x00})});
    });
    _daemon.on(gatt_descriptor_interface, "WriteValue", [&](const dbus::Message & call) {
        written = dbus::Iter(call).get_array<uint8_t>().value_or(std::vector<uint8_t> {});
        _daemon.reply(call, {});
    });
    _daemon.serve();
    ASSERT_NO_FATAL_FAILURE(this->connect_and_list());
    // the client characteristic configuration enabling notifications
    const std::vector<uint8_t> enable_notifications {0x01, 0x00};

    auto read = _device.read_descriptor(cccd);
    auto write = _device.write_descriptor(cccd, enable_notifications);
    this->disconnect();
    _daemon.stop();

    ASSERT_TRUE(read.has_value()) << read.error().message;
    EXPECT_EQ(*read, (std::vector<uint8_t> {0x00, 0x00}));
    EXPECT_TRUE(write.has_value()) << write.error().message;
    EXPECT_EQ(written, enable_notifications);
}

TEST_F(TestBtDevice, test_subscribe_acquires_notifications)
{
    // bluez hands a non-blocking socket, one datagram per notification
    SocketPair notifications(SOCK_SEQPACKET | SOCK_NONBLOCK);
    _daemon.on(gatt_characteristic_interface, "AcquireNotify", [this, &notifications](const dbus::Message & call) {
        _daemon.reply(call, {unix_fd(notifications.local()), u16(23)});
        EXPECT_TRUE(notifications.write_remote("\x51"));
    });
    _daemon.serve();
    Synchronizer notified;
    Handler<Device *> waiter;
    this->wait_notification(notified, waiter);
    ASSERT_NO_FATAL_FAILURE(this->connect_and_list());

    auto subscribed = _device.subscribe(level);
    const bool synced = notified.sync(Duration(time::sec(5)));
    const std::optional<GattNotification> notification = _device.last_notification();
    auto unsubscribed = _device.unsubscribe(level);
    this->disconnect();

    ASSERT_TRUE(subscribed.has_value()) << subscribed.error().message;
    EXPECT_TRUE(synced);
    ASSERT_TRUE(notification.has_value());
    EXPECT_EQ(notification->handle, 0x000d);
    EXPECT_EQ(notification->uuid, level_uuid);
    EXPECT_EQ(notification->value, std::vector<uint8_t> {0x51});
    EXPECT_TRUE(unsubscribed.has_value()) << unsubscribed.error().message;
    EXPECT_EQ(_events,
              (std::vector<Device::Event> {Device::Event::connected,
                                           Device::Event::notification,
                                           Device::Event::disconnected}));
}

TEST_F(TestBtDevice, test_subscribe_indications)
{
    _daemon.on(gatt_characteristic_interface, "StartNotify", [this](const dbus::Message & call) {
        _daemon.reply(call, {});
        _daemon.emit_properties_changed(alert_path,
                                        gatt_characteristic_interface,
                                        {{"Value", bytes(std::vector<uint8_t> {0x02})}});
    });
    answer(_daemon, gatt_characteristic_interface, {"StopNotify"});
    _daemon.serve();
    Synchronizer notified;
    Handler<Device *> waiter;
    this->wait_notification(notified, waiter);
    ASSERT_NO_FATAL_FAILURE(this->connect_and_list());

    auto subscribed = _device.subscribe(alert);
    const bool synced = notified.sync(Duration(time::sec(5)));
    const std::optional<GattNotification> notification = _device.last_notification();
    auto unsubscribed = _device.unsubscribe(alert);
    this->disconnect();
    _daemon.stop();

    ASSERT_TRUE(subscribed.has_value()) << subscribed.error().message;
    EXPECT_TRUE(synced);
    ASSERT_TRUE(notification.has_value());
    EXPECT_EQ(notification->handle, 0x0010);
    EXPECT_EQ(notification->value, std::vector<uint8_t> {0x02});
    EXPECT_TRUE(unsubscribed.has_value()) << unsubscribed.error().message;
    EXPECT_TRUE(_daemon.called(gatt_characteristic_interface, "StopNotify"));
}

TEST_F(TestBtDevice, test_mtu)
{
    _daemon.serve();
    ASSERT_NO_FATAL_FAILURE(this->connect());

    auto mtu = _device.mtu();
    this->disconnect();

    ASSERT_TRUE(mtu.has_value()) << mtu.error().message;
    EXPECT_EQ(*mtu, 247);
}

TEST_F(TestBtDevice, test_pair_with_handler)
{
    this->serve_pairing(true);
    _daemon.serve();
    std::vector<PairingRequest> requests;
    _device.set_pairing_handler([&requests](const PairingRequest & request) {
        requests.push_back(request);
        return PairingAnswer {.accept = true, .passkey = 0, .pin = ""};
    });

    auto paired = _device.pair();
    _daemon.stop();

    EXPECT_TRUE(paired.has_value()) << paired.error().message;
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_EQ(requests[0].address, device_address);
    EXPECT_EQ(requests[0].kind, PairingRequest::Kind::confirm_passkey);
    EXPECT_EQ(requests[0].passkey, 123456u);
    EXPECT_TRUE(_daemon.called(agent_manager_interface, "UnregisterAgent"));
}

TEST_F(TestBtDevice, test_pair_rejected)
{
    this->serve_pairing(true);
    _daemon.serve();
    _device.set_pairing_handler(
        [](const PairingRequest &) { return PairingAnswer {.accept = false, .passkey = 0, .pin = ""}; });

    auto paired = _device.pair();
    _daemon.stop();

    ASSERT_FALSE(paired.has_value());
    EXPECT_EQ(paired.error().code, ErrorCode::permission_denied);
    EXPECT_EQ(paired.error().message, "org.bluez.Error.AuthenticationRejected: Authentication Rejected");
    EXPECT_TRUE(_daemon.called(agent_manager_interface, "UnregisterAgent"));
}

TEST_F(TestBtDevice, test_pair_through_system_agent)
{
    this->serve_pairing(false);
    _daemon.serve();

    auto paired = _device.pair();
    _daemon.stop();

    EXPECT_TRUE(paired.has_value()) << paired.error().message;
    EXPECT_FALSE(_daemon.called(agent_manager_interface, "RegisterAgent"));
}

TEST_F(TestBtDevice, test_cancel_pairing)
{
    std::string canceled_path;
    _daemon.on(device_interface, "CancelPairing", [&](const dbus::Message & call) {
        canceled_path = call.path();
        _daemon.reply(call, {});
    });
    _daemon.serve();

    auto canceled = _device.cancel_pairing();
    _daemon.stop();

    EXPECT_TRUE(canceled.has_value()) << canceled.error().message;
    EXPECT_EQ(canceled_path, device_path);
}

TEST_F(TestBtDevice, test_cancel_without_pairing)
{
    _daemon.on(device_interface, "CancelPairing", [this](const dbus::Message & call) {
        _daemon.fail(call, "org.bluez.Error.DoesNotExist", "Does Not Exist");
    });
    _daemon.serve();

    auto canceled = _device.cancel_pairing();
    _daemon.stop();

    ASSERT_FALSE(canceled.has_value());
    EXPECT_EQ(canceled.error().code, ErrorCode::not_found);
    EXPECT_EQ(canceled.error().message, "org.bluez.Error.DoesNotExist: Does Not Exist");
}

TEST_F(TestBtDevice, test_unpair)
{
    std::string removed;
    _daemon.on(adapter_interface, "RemoveDevice", [&](const dbus::Message & call) {
        removed = dbus::Iter(call).get<std::string>().value_or("");
        _daemon.reply(call, {});
    });
    _daemon.serve();

    auto unpaired = _device.unpair();
    _daemon.stop();

    EXPECT_TRUE(unpaired.has_value()) << unpaired.error().message;
    EXPECT_EQ(removed, device_path);
}

TEST_F(TestBtDevice, test_set_trusted)
{
    _daemon.serve();

    auto trusted = _device.set_trusted(true);
    auto info = _device.info();

    EXPECT_TRUE(trusted.has_value()) << trusted.error().message;
    ASSERT_TRUE(info.has_value()) << info.error().message;
    EXPECT_TRUE(info->trusted);
}

} // namespace test
