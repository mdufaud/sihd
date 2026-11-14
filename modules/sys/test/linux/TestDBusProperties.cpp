#include <functional>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys::linux_::dbus;

class TestDBusProperties: public ::testing::Test
{
    protected:
        TestDBusProperties() { LoggerManager::stream(); }

        virtual ~TestDBusProperties() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!Connection::supported)
            {
                GTEST_SKIP() << "dbus not supported on this platform";
            }
            if (!PrivateDBus::available())
            {
                GTEST_SKIP() << "no dbus-daemon";
            }
            ASSERT_TRUE(_bus.start());
            auto server_opened = _server.open(_bus.address());
            ASSERT_TRUE(server_opened.has_value()) << server_opened.error().message;
            auto client_opened = _client.open(_bus.address());
            ASSERT_TRUE(client_opened.has_value()) << client_opened.error().message;
            ASSERT_TRUE(_server.request_name(service).has_value());
        }

        virtual void TearDown() {}

        // the server answers the next call of that member while the test thread calls
        std::jthread answer_next(const std::string & member, const std::function<void(const Message &)> & answer)
        {
            return std::jthread([this, member, answer] {
                auto call = receive_member(_server, member);
                ASSERT_TRUE(call.has_value());
                answer(*call);
            });
        }

        void reply(const Message & call, const std::function<Result<void>(Appender &)> & fill)
        {
            auto answer = call.method_return();
            ASSERT_TRUE(answer.has_value()) << answer.error().message;
            Appender body(*answer);
            auto filled = fill(body);
            ASSERT_TRUE(filled.has_value()) << filled.error().message;
            ASSERT_TRUE(answer->send(_server).has_value());
        }

        static Result<Message> properties_changed(ZStringView interface)
        {
            Message signal;
            auto built = signal.signal(path, "org.freedesktop.DBus.Properties", "PropertiesChanged");
            SIHD_UNEXPECTED_RETURN(built);

            auto added = signal.add(interface, Dict {{"State", "connected"}}, std::vector<std::string> {"Strength"});
            SIHD_UNEXPECTED_RETURN(added);
            return signal;
        }

        static constexpr ZStringView service = "org.sihd.Test";
        static constexpr ZStringView path = "/org/sihd/Device/1";
        static constexpr ZStringView interface = "org.sihd.Device";
        inline static const Duration timeout = Duration(time::sec(5));

        PrivateDBus _bus;
        Connection _server;
        Connection _client;
};

TEST_F(TestDBusProperties, test_get_property)
{
    std::vector<std::string> asked;
    auto server = this->answer_next("Get", [this, &asked](const Message & call) {
        auto [interface_name, property_name] = Iter(call).read<std::string, std::string>().value_or(
            std::tuple<std::string, std::string> {});
        asked.emplace_back(call.path());
        asked.emplace_back(interface_name);
        asked.emplace_back(property_name);
        this->reply(call, [](Appender & body) -> Result<void> {
            auto variant = body.open_variant("u");
            SIHD_UNEXPECTED_RETURN(variant);
            auto added = variant->add(uint32_t {70});
            SIHD_UNEXPECTED_RETURN(added);
            return body.close(std::move(*variant));
        });
    });

    auto strength = get_property<uint32_t>(_client, service, path, interface, "Strength", timeout);
    server.join();

    ASSERT_TRUE(strength.has_value()) << strength.error().message;
    EXPECT_EQ(*strength, 70u);
    EXPECT_EQ(asked, (std::vector<std::string> {"/org/sihd/Device/1", "org.sihd.Device", "Strength"}));
}

TEST_F(TestDBusProperties, test_get_array_property)
{
    auto server = this->answer_next("Get", [this](const Message & call) {
        this->reply(call, [](Appender & body) -> Result<void> {
            return body.add(Variant(std::vector<uint8_t> {'h', 'o', 'm', 'e'}));
        });
    });

    auto ssid = get_array_property<uint8_t>(_client, service, path, interface, "Ssid", timeout);
    server.join();

    ASSERT_TRUE(ssid.has_value()) << ssid.error().message;
    EXPECT_EQ(*ssid, (std::vector<uint8_t> {'h', 'o', 'm', 'e'}));
}

TEST_F(TestDBusProperties, test_set_property)
{
    std::vector<std::string> asked;
    std::optional<bool> written;
    auto server = this->answer_next("Set", [this, &asked, &written](const Message & call) {
        Iter arguments(call);
        asked.emplace_back(arguments.get<std::string>().value_or(""));
        arguments.next();
        asked.emplace_back(arguments.get<std::string>().value_or(""));
        arguments.next();
        asked.emplace_back(arguments.signature());
        auto value = arguments.recurse();
        if (value.has_value())
            written = value->get<bool>().value_or(false);
        this->reply(call, [](Appender &) -> Result<void> { return {}; });
    });

    auto set = set_property(_client, service, path, interface, "Powered", true, timeout);
    server.join();

    ASSERT_TRUE(set.has_value()) << set.error().message;
    EXPECT_EQ(asked, (std::vector<std::string> {"org.sihd.Device", "Powered", "v"}));
    EXPECT_EQ(written, std::optional<bool>(true));
}

TEST_F(TestDBusProperties, test_set_property_error)
{
    auto server = this->answer_next("Set", [this](const Message & call) {
        auto refusal = call.error_return("org.freedesktop.DBus.Error.PropertyReadOnly", "Address is read-only");
        ASSERT_TRUE(refusal.has_value()) << refusal.error().message;
        ASSERT_TRUE(refusal->send(_server).has_value());
    });

    auto set = set_property<std::string>(_client, service, path, interface, "Address", "00:11:22:33:44:55", timeout);
    server.join();

    ASSERT_FALSE(set.has_value());
    EXPECT_EQ(set.error().code, ErrorCode::io_error);
    EXPECT_EQ(set.error().message, "org.freedesktop.DBus.Error.PropertyReadOnly: Address is read-only");
}

TEST_F(TestDBusProperties, test_get_property_error)
{
    auto server = this->answer_next("Get", [this](const Message & call) {
        auto refusal = call.error_return("org.freedesktop.DBus.Error.UnknownProperty", "no Mode");
        ASSERT_TRUE(refusal.has_value()) << refusal.error().message;
        ASSERT_TRUE(refusal->send(_server).has_value());
    });

    auto mode = get_property<std::string>(_client, service, path, interface, "Mode", timeout);
    server.join();

    ASSERT_FALSE(mode.has_value());
    EXPECT_EQ(mode.error().code, ErrorCode::io_error);
    EXPECT_EQ(mode.error().message, "org.freedesktop.DBus.Error.UnknownProperty: no Mode");
    EXPECT_TRUE(has_error_name(mode.error(), "org.freedesktop.DBus.Error.UnknownProperty"));
    EXPECT_FALSE(has_error_name(mode.error(), "org.freedesktop.DBus.Error.Unknown"));
}

TEST_F(TestDBusProperties, test_get_all)
{
    std::string asked;
    auto server = this->answer_next("GetAll", [this, &asked](const Message & call) {
        asked = Iter(call).get<std::string>().value_or("");
        this->reply(call, [](Appender & body) -> Result<void> {
            return body.add(Dict {{"Name", "wlan0"}, {"Managed", true}});
        });
    });

    Message request;
    auto built = request.properties_get_all(service, path, interface);
    ASSERT_TRUE(built.has_value()) << built.error().message;
    auto reply = request.call(_client, timeout);
    server.join();

    ASSERT_TRUE(reply.has_value()) << reply.error().message;
    EXPECT_EQ(asked, interface);
    auto properties = Iter(*reply).entries<std::string>();
    ASSERT_TRUE(properties.has_value()) << properties.error().message;
    std::map<std::string, char> types;
    for (auto & [name, value] : *properties)
        types.emplace(name, value.type());
    EXPECT_EQ(types, (std::map<std::string, char> {{"Managed", 'b'}, {"Name", 's'}}));
}

TEST_F(TestDBusProperties, test_changed_property)
{
    auto signal = properties_changed(interface);
    ASSERT_TRUE(signal.has_value()) << signal.error().message;
    auto other_interface = properties_changed("org.sihd.Other");
    ASSERT_TRUE(other_interface.has_value()) << other_interface.error().message;
    Message call;
    auto call_built = call.method_call(service, path, "org.freedesktop.DBus.Properties", "PropertiesChanged");
    ASSERT_TRUE(call_built.has_value()) << call_built.error().message;
    ASSERT_TRUE(call.add(interface).has_value());

    auto state = signal->changed_property(interface, "State");
    auto strength = signal->changed_property(interface, "Strength");
    auto elsewhere = other_interface->changed_property(interface, "State");
    auto not_a_signal = call.changed_property(interface, "State");

    ASSERT_TRUE(state.has_value()) << state.error().message;
    ASSERT_TRUE(state->has_value());
    EXPECT_EQ((*state)->get<std::string>().value_or(""), "connected");
    ASSERT_TRUE(strength.has_value()) << strength.error().message;
    EXPECT_FALSE(strength->has_value());
    ASSERT_TRUE(elsewhere.has_value()) << elsewhere.error().message;
    EXPECT_FALSE(elsewhere->has_value());
    ASSERT_TRUE(not_a_signal.has_value()) << not_a_signal.error().message;
    EXPECT_FALSE(not_a_signal->has_value());
}

TEST_F(TestDBusProperties, test_properties_changed_rule)
{
    const std::string rule = properties_changed_rule(service, path, interface);
    ASSERT_TRUE(_client.add_match(rule).has_value());

    auto other_interface = properties_changed("org.sihd.Other");
    ASSERT_TRUE(other_interface.has_value()) << other_interface.error().message;
    ASSERT_TRUE(other_interface->send(_server).has_value());
    auto signal = properties_changed(interface);
    ASSERT_TRUE(signal.has_value()) << signal.error().message;
    ASSERT_TRUE(signal->send(_server).has_value());
    auto received = receive_member(_client, "PropertiesChanged");

    EXPECT_EQ(rule,
              "type='signal',sender='org.sihd.Test',path='/org/sihd/Device/1',"
              "interface='org.freedesktop.DBus.Properties',member='PropertiesChanged',arg0='org.sihd.Device'");
    EXPECT_EQ(signal_rule(service, path, interface, "StateChanged"),
              "type='signal',sender='org.sihd.Test',path='/org/sihd/Device/1',interface='org.sihd.Device',"
              "member='StateChanged'");
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(Iter(*received).get<std::string>().value_or(""), interface);
}

TEST_F(TestDBusProperties, test_add_array)
{
    Message message;
    auto built = message.method_call(service, path, interface, "Set");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    Appender body(message);

    ASSERT_TRUE(body.add(std::vector<std::string> {"first", "second"}).has_value());
    ASSERT_TRUE(body.add(std::vector<bool> {true, false}).has_value());
    ASSERT_TRUE(body.add(std::vector<int32_t> {}).has_value());

    Iter arguments(message);
    EXPECT_EQ(arguments.get_array<std::string>().value_or(std::vector<std::string> {}),
              (std::vector<std::string> {"first", "second"}));
    arguments.next();
    EXPECT_EQ(arguments.get_array<bool>().value_or(std::vector<bool> {}), (std::vector<bool> {true, false}));
    arguments.next();
    EXPECT_EQ(arguments.type(), 'a');
    EXPECT_EQ(arguments.get_array<int32_t>().value_or(std::vector<int32_t> {1}), std::vector<int32_t> {});
}

TEST_F(TestDBusProperties, test_entries_needs_a_dict)
{
    Message message;
    auto built = message.method_call(service, path, interface, "Set");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    ASSERT_TRUE(message.add("text").has_value());
    Appender body(message);
    auto record = body.open_struct();
    ASSERT_TRUE(record.has_value()) << record.error().message;
    ASSERT_TRUE(record->add(uint32_t {1}).has_value());
    ASSERT_TRUE(body.close(std::move(*record)).has_value());

    Iter arguments(message);
    auto from_text = arguments.entries<std::string>();
    arguments.next();
    auto from_struct = arguments.entries<std::string>();

    ASSERT_FALSE(from_text.has_value());
    EXPECT_EQ(from_text.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(from_text.error().message, "dbus type 's' is not a dict");
    ASSERT_FALSE(from_struct.has_value());
    EXPECT_EQ(from_struct.error().message, "dbus type 'r' is not a dict");
}

TEST_F(TestDBusProperties, test_entries_checks_the_key_type)
{
    auto signal = properties_changed(interface);
    ASSERT_TRUE(signal.has_value()) << signal.error().message;
    Iter arguments(*signal);
    arguments.next();

    auto by_number = arguments.entries<uint32_t>();

    ASSERT_FALSE(by_number.has_value());
    EXPECT_EQ(by_number.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(by_number.error().message, "dbus type mismatch: expected 'u', got 's'");
}

} // namespace test
