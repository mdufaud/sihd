#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/bt/GattServer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "fake_bluez.hpp"

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;

inline constexpr char echo_service_uuid[] = "12345678-1234-5678-1234-56789abcdef0";
inline constexpr char echo_uuid[] = "12345678-1234-5678-1234-56789abcdef1";
inline constexpr char answer_uuid[] = "12345678-1234-5678-1234-56789abcdef2";
inline constexpr char single_byte_uuid[] = "12345678-1234-5678-1234-56789abcdef3";
inline constexpr char application_path[] = "/org/sihd/bt/application";
inline constexpr char echo_path[] = "/org/sihd/bt/application/service0/char0";
inline constexpr char answer_path[] = "/org/sihd/bt/application/service0/char1";
inline constexpr char single_byte_path[] = "/org/sihd/bt/application/service0/char2";

class TestBtGattServer: public ::testing::Test
{
    protected:
        TestBtGattServer(): _server("server") { LoggerManager::stream(); }

        virtual ~TestBtGattServer() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if (!FakeDaemon::available())
                GTEST_SKIP() << "no dbus-daemon";
            ASSERT_TRUE(_daemon.start({bluez_service}));
            serve_adapter(_daemon, adapter_path, true);
            // bluez reads the application back before RegisterApplication answers
            _daemon.on(gatt_manager_interface, "RegisterApplication", [this](const dbus::Message & call) {
                _application_sender = call.sender();
                _registered_path = dbus::Iter(call).get<std::string>().value_or("");
                if (_on_register)
                    _on_register();
                _daemon.reply(call, {});
            });
            answer(_daemon, gatt_manager_interface, {"UnregisterApplication"});
            ASSERT_TRUE(_server.add_service(LocalService {
                .uuid = echo_service_uuid,
                .primary = true,
                .characteristics =
                    {
                        LocalCharacteristic {
                            .uuid = echo_uuid,
                            .properties = GattCharacteristic::read | GattCharacteristic::write
                                          | GattCharacteristic::notify,
                            .on_read = {},
                            .on_write = {},
                        },
                        LocalCharacteristic {
                            .uuid = answer_uuid,
                            .properties = GattCharacteristic::read,
                            .on_read = [] { return std::vector<uint8_t> {0x2a}; },
                            .on_write = {},
                        },
                        LocalCharacteristic {
                            .uuid = single_byte_uuid,
                            .properties = GattCharacteristic::write,
                            .on_read = {},
                            .on_write = [](const std::vector<uint8_t> & value) { return value.size() == 1; },
                        },
                    },
            }));
        }

        void start()
        {
            _serving = std::jthread([this] { _started = _server.start(); });
            ASSERT_TRUE(_server.wait_ready(Duration(time::sec(5))));
        }

        void stop()
        {
            _server.stop();
            _serving.join();
        }

        // from a handler: a method of the application, as a remote device reaches it through bluez
        Result<dbus::Message> call_application(const std::string & path,
                                               const std::string & member,
                                               const std::function<Result<void>(dbus::Appender &)> & arguments)
        {
            dbus::Message request;
            auto built = request.method_call(_application_sender, path, gatt_characteristic_interface, member);
            SIHD_UNEXPECTED_RETURN(built);
            dbus::Appender body(request);
            auto filled = arguments(body);
            SIHD_UNEXPECTED_RETURN(filled);
            return request.call(_daemon.connection(), fake_timeout);
        }

        // ReadValue's answer, or the name of the error refusing it
        std::string remote_read(const std::string & path, uint16_t offset)
        {
            auto reply = this->call_application(path, "ReadValue", [offset](dbus::Appender & to) {
                return append_entries(to, {{"offset", u16(offset)}});
            });
            if (!reply)
                return reply.error().message;
            auto value = dbus::Iter(*reply).get_array<uint8_t>();
            return value ? std::string(value->begin(), value->end()) : value.error().message;
        }

        std::string remote_write(const std::string & path, const std::string & value)
        {
            auto called = this->call_application(path, "WriteValue", [&value](dbus::Appender & to) -> Result<void> {
                auto added = to.add_array(std::vector<uint8_t>(value.begin(), value.end()));
                SIHD_UNEXPECTED_RETURN(added);
                return append_entries(to, {});
            });
            return called ? "written" : called.error().message;
        }

        FakeDaemon _daemon;
        std::function<void()> _on_register;
        std::string _application_sender;
        std::string _registered_path;
        GattServer _server;
        bool _started = false;
        std::jthread _serving;
};

TEST_F(TestBtGattServer, test_publishes_application)
{
    std::map<std::string, std::map<std::string, ReadProperties>> objects;
    _on_register = [this, &objects] {
        dbus::Message request;
        auto built = request.method_call(_application_sender,
                                         application_path,
                                         object_manager_interface,
                                         "GetManagedObjects");
        ASSERT_TRUE(built.has_value()) << built.error().message;
        auto reply = request.call(_daemon.connection(), fake_timeout);
        ASSERT_TRUE(reply.has_value()) << reply.error().message;
        auto parsed = dbus::Iter(*reply).for_each_entry<std::string>(
            [&objects](const std::string & path, dbus::Iter & interfaces) {
                return interfaces.for_each_entry<std::string>(
                    [&objects, &path](const std::string & interface, dbus::Iter & properties) {
                        return read_properties(properties, objects[path][interface]);
                    });
            });
        ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
    };
    _daemon.serve();

    ASSERT_NO_FATAL_FAILURE(this->start());
    this->stop();
    _daemon.stop();

    EXPECT_TRUE(_started);
    EXPECT_EQ(_registered_path, application_path);
    const ReadProperties & service = objects["/org/sihd/bt/application/service0"][gatt_service_interface];
    EXPECT_EQ(service.texts.at("UUID"), echo_service_uuid);
    EXPECT_TRUE(service.booleans.at("Primary"));
    const ReadProperties & echo = objects[echo_path][gatt_characteristic_interface];
    EXPECT_EQ(echo.texts.at("UUID"), echo_uuid);
    EXPECT_EQ(echo.texts.at("Service"), "/org/sihd/bt/application/service0");
    EXPECT_EQ(echo.arrays.at("Flags"), (std::vector<std::string> {"read", "write", "notify"}));
    EXPECT_EQ(objects[answer_path][gatt_characteristic_interface].arrays.at("Flags"),
              std::vector<std::string> {"read"});
    EXPECT_EQ(objects[single_byte_path][gatt_characteristic_interface].arrays.at("Flags"),
              std::vector<std::string> {"write"});
    EXPECT_TRUE(_daemon.called(gatt_manager_interface, "UnregisterApplication"));
}

TEST_F(TestBtGattServer, test_remote_reads_and_writes)
{
    std::vector<std::string> answers;
    _on_register = [this, &answers] {
        answers.push_back(this->remote_write(echo_path, "ab"));
        answers.push_back(this->remote_read(echo_path, 0));
        answers.push_back(this->remote_read(echo_path, 1));
        answers.push_back(this->remote_read(echo_path, 3));
        answers.push_back(this->remote_read(answer_path, 0));
        answers.push_back(this->remote_write(single_byte_path, "xy"));
        answers.push_back(this->remote_write(single_byte_path, "x"));
    };
    _daemon.serve();

    ASSERT_NO_FATAL_FAILURE(this->start());
    this->stop();

    EXPECT_EQ(answers,
              (std::vector<std::string> {
                  "written",
                  "ab",
                  "b",
                  "org.bluez.Error.InvalidOffset: read past the value",
                  "*",
                  "org.bluez.Error.NotPermitted: write refused",
                  "written",
              }));
}

TEST_F(TestBtGattServer, test_notify)
{
    dbus::Connection observer;
    ASSERT_TRUE(observer.open(shared_bus()->address()).has_value());
    ASSERT_TRUE(observer
                    .add_match("type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',"
                               "path_namespace='/org/sihd/bt/application'")
                    .has_value());
    _daemon.serve();
    const std::vector<uint8_t> value {0x10, 0x20};

    ASSERT_NO_FATAL_FAILURE(this->start());
    auto notified = _server.notify(echo_service_uuid, "12345678-1234-5678-1234-56789ABCDEF1", value);
    dbus::Message signal;
    auto received = signal.receive_until(observer, Duration(time::sec(5)), [](const dbus::Message & message) {
        return Result<bool>(message.member() == "PropertiesChanged");
    });
    this->stop();

    ASSERT_TRUE(notified.has_value()) << notified.error().message;
    ASSERT_TRUE(received.has_value()) << received.error().message;
    EXPECT_EQ(signal.path(), echo_path);
    auto changed = signal.changed_property(gatt_characteristic_interface, "Value");
    ASSERT_TRUE(changed.has_value() && changed->has_value());
    EXPECT_EQ((*changed)->get_array<uint8_t>().value_or(std::vector<uint8_t> {}), value);
}

TEST_F(TestBtGattServer, test_notify_errors)
{
    _daemon.serve();
    const std::vector<uint8_t> value {0x01};

    auto before_start = _server.notify(echo_service_uuid, echo_uuid, value);
    ASSERT_NO_FATAL_FAILURE(this->start());
    auto unknown = _server.notify(echo_service_uuid, battery_uuid, value);
    auto read_only = _server.notify(echo_service_uuid, answer_uuid, value);
    this->stop();

    ASSERT_FALSE(before_start.has_value());
    EXPECT_EQ(before_start.error().code, ErrorCode::closed);
    EXPECT_EQ(before_start.error().message, "gatt server is not started");
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);
    EXPECT_EQ(unknown.error().message,
              "no characteristic 0000180f-0000-1000-8000-00805f9b34fb in service 12345678-1234-5678-1234-56789abcdef0");
    ASSERT_FALSE(read_only.has_value());
    EXPECT_EQ(read_only.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(read_only.error().message,
              "characteristic 12345678-1234-5678-1234-56789abcdef2 neither notifies nor indicates");
}

TEST_F(TestBtGattServer, test_registration_refused)
{
    _daemon.on(gatt_manager_interface, "RegisterApplication", [this](const dbus::Message & call) {
        _daemon.fail(call, "org.bluez.Error.AlreadyExists", "Already Exists");
    });
    _daemon.serve();

    const bool started = _server.start();
    _daemon.stop();

    EXPECT_FALSE(started);
    EXPECT_FALSE(_daemon.called(gatt_manager_interface, "UnregisterApplication"));
}

TEST_F(TestBtGattServer, test_services_kept_while_serving)
{
    _daemon.serve();

    ASSERT_NO_FATAL_FAILURE(this->start());
    const bool added = _server.add_service(LocalService {.uuid = battery_uuid, .primary = true, .characteristics = {}});
    this->stop();

    EXPECT_FALSE(added);
}

} // namespace test
