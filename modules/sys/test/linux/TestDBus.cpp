#include <algorithm>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/sys/env.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/time.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_::dbus;

class TestDBus: public ::testing::Test
{
    protected:
        TestDBus() { LoggerManager::stream(); }

        virtual ~TestDBus() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!Connection::supported)
            {
                GTEST_SKIP() << "dbus not supported on this platform";
            }
            if (!env::get("DBUS_SESSION_BUS_ADDRESS").has_value())
            {
                GTEST_SKIP() << "no dbus session bus";
            }
            auto opened = _connection.open(Bus::session);
            ASSERT_TRUE(opened.has_value()) << opened.error().message;
        }

        virtual void TearDown() {}

        Result<Message> new_bus_call(const std::string & member)
        {
            return Message::method_call("org.freedesktop.DBus",
                                        "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus",
                                        member);
        }

        // skips what the bus sends unasked (NameAcquired...): nullopt on timeout or interrupt
        static Result<std::optional<Message>>
            receive_member(Connection & connection, const std::string & member, Duration timeout)
        {
            while (true)
            {
                auto received = Message::receive(connection, timeout);
                if (!received.has_value() || !received->has_value() || (*received)->member() == member)
                    return received;
            }
        }

        Connection _connection;
};

TEST_F(TestDBus, test_list_names)
{
    auto request = this->new_bus_call("ListNames");
    ASSERT_TRUE(request.has_value()) << request.error().message;

    auto reply = request->call(_connection, Duration(time::sec(5)));
    ASSERT_TRUE(reply.has_value()) << reply.error().message;

    auto names = Iter(*reply).get_array<std::string>();
    ASSERT_TRUE(names.has_value()) << names.error().message;

    EXPECT_NE(std::ranges::find(*names, "org.freedesktop.DBus"), names->end());
}

TEST_F(TestDBus, test_call_with_argument)
{
    auto request = this->new_bus_call("NameHasOwner");
    ASSERT_TRUE(request.has_value()) << request.error().message;
    ASSERT_TRUE(request->add("org.freedesktop.DBus").has_value());

    auto reply = request->call(_connection, Duration(time::sec(5)));
    ASSERT_TRUE(reply.has_value()) << reply.error().message;

    Iter body(*reply);
    auto has_owner = body.get<bool>();
    ASSERT_TRUE(has_owner.has_value()) << has_owner.error().message;
    EXPECT_EQ(*has_owner, true);
}

TEST_F(TestDBus, test_error_reply)
{
    auto request = this->new_bus_call("NoSuchMethod");
    ASSERT_TRUE(request.has_value()) << request.error().message;

    auto reply = request->call(_connection, Duration(time::sec(5)));

    ASSERT_FALSE(reply.has_value());
    EXPECT_EQ(reply.error().code, ErrorCode::io_error);
    EXPECT_TRUE(reply.error().message.starts_with("org.freedesktop.DBus.Error.UnknownMethod: "))
        << reply.error().message;
}

TEST_F(TestDBus, test_read_back_arguments)
{
    auto message = this->new_bus_call("Ping");
    ASSERT_TRUE(message.has_value()) << message.error().message;
    ASSERT_TRUE(message->add("text").has_value());
    ASSERT_TRUE(message->add(true).has_value());
    ASSERT_TRUE(message->add(int32_t {-5}).has_value());
    ASSERT_TRUE(message->add(uint32_t {7}).has_value());
    ASSERT_TRUE(message->add(uint8_t {0xab}).has_value());
    ASSERT_TRUE(message->add(int16_t {-300}).has_value());
    ASSERT_TRUE(message->add(uint16_t {60000}).has_value());

    Iter iter(*message);

    EXPECT_EQ(iter.get<std::string>().value_or(""), "text");
    EXPECT_TRUE(iter.next());
    EXPECT_EQ(iter.get<bool>().value_or(false), true);
    EXPECT_TRUE(iter.next());
    EXPECT_EQ(iter.get<int32_t>().value_or(0), -5);
    EXPECT_TRUE(iter.next());
    EXPECT_EQ(iter.get<uint32_t>().value_or(0), 7u);

    EXPECT_TRUE(iter.next());
    EXPECT_EQ(iter.get<uint8_t>().value_or(0), 0xab);
    EXPECT_TRUE(iter.next());
    EXPECT_EQ(iter.get<int16_t>().value_or(0), -300);
    EXPECT_TRUE(iter.next());
    EXPECT_EQ(iter.get<uint16_t>().value_or(0), 60000);
    EXPECT_FALSE(iter.next());
    EXPECT_EQ(iter.type(), '\0');
}

TEST_F(TestDBus, test_type_mismatch)
{
    auto message = this->new_bus_call("Ping");
    ASSERT_TRUE(message.has_value()) << message.error().message;
    ASSERT_TRUE(message->add("text").has_value());

    Iter iter(*message);
    auto as_int = iter.get<int32_t>();
    auto as_container = iter.recurse();

    ASSERT_FALSE(as_int.has_value());
    EXPECT_EQ(as_int.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(as_int.error().message, "dbus type mismatch: expected 'i', got 's'");
    ASSERT_FALSE(as_container.has_value());
    EXPECT_EQ(as_container.error().message, "dbus type 's' is not a container");
    auto as_array = iter.get_array<std::string>();
    ASSERT_FALSE(as_array.has_value());
    EXPECT_EQ(as_array.error().message, "dbus type 's' is not a container");
}

TEST_F(TestDBus, test_invalid_path)
{
    auto bad_path = Message::method_call("org.freedesktop.DBus", "no slash", "org.freedesktop.DBus", "Ping");

    ASSERT_FALSE(bad_path.has_value());
    EXPECT_EQ(bad_path.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(bad_path.error().message, "invalid dbus path: no slash");
}

TEST_F(TestDBus, test_call_on_closed_connection)
{
    auto request = this->new_bus_call("Ping");
    ASSERT_TRUE(request.has_value()) << request.error().message;
    _connection.close();

    auto reply = request->call(_connection, Duration(time::sec(5)));

    EXPECT_FALSE(_connection.is_open());
    ASSERT_FALSE(reply.has_value());
    EXPECT_EQ(reply.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(reply.error().message, "dbus connection is not open");
}

TEST_F(TestDBus, test_signal_roundtrip)
{
    Connection listener;
    ASSERT_TRUE(listener.open(Bus::session).has_value());
    ASSERT_TRUE(listener.add_match("type='signal',interface='org.sihd.Test',member='Hello'").has_value());
    auto signal = Message::signal("/org/sihd/Test", "org.sihd.Test", "Hello");
    ASSERT_TRUE(signal.has_value()) << signal.error().message;
    ASSERT_TRUE(signal->add("payload").has_value());

    ASSERT_TRUE(signal->send(_connection).has_value());
    auto received = receive_member(listener, "Hello", Duration(time::sec(5)));

    ASSERT_TRUE(received.has_value()) << received.error().message;
    ASSERT_TRUE(received->has_value());
    const Message & message = **received;
    EXPECT_EQ(message.path(), "/org/sihd/Test");
    EXPECT_EQ(message.interface(), "org.sihd.Test");
    EXPECT_TRUE(message.sender().starts_with(":")) << message.sender();
    EXPECT_EQ(Iter(message).get<std::string>().value_or(""), "payload");
    EXPECT_TRUE(listener.remove_match("type='signal',interface='org.sihd.Test',member='Hello'").has_value());
}

TEST_F(TestDBus, test_receive_timeout_and_interrupt)
{
    Stopwatch timeout_watch;
    auto timed_out = receive_member(_connection, "Never", Duration(time::milli(100)));
    const Duration timeout_elapsed = timeout_watch.time();

    ASSERT_TRUE(timed_out.has_value()) << timed_out.error().message;
    EXPECT_FALSE(timed_out->has_value());
    EXPECT_GE(timeout_elapsed, Duration(time::milli(100)));

    Stopwatch interrupt_watch;
    Result<std::optional<Message>> interrupted = std::nullopt;
    std::jthread receiver(
        [this, &interrupted] { interrupted = receive_member(_connection, "Never", Duration(time::sec(10))); });
    ASSERT_TRUE(_connection.interrupt().has_value());
    receiver.join();

    ASSERT_TRUE(interrupted.has_value()) << interrupted.error().message;
    EXPECT_FALSE(interrupted->has_value());
    EXPECT_LT(interrupt_watch.time(), Duration(time::sec(5)));
}

TEST_F(TestDBus, test_invalid_match_and_signal)
{
    auto bad_rule = _connection.add_match("not a rule");
    auto no_interface = Message::signal("/org/sihd/Test", "", "Hello");

    ASSERT_FALSE(bad_rule.has_value());
    EXPECT_EQ(bad_rule.error().code, ErrorCode::io_error);
    EXPECT_TRUE(bad_rule.error().message.starts_with("org.freedesktop.DBus.Error.MatchRuleInvalid: "))
        << bad_rule.error().message;
    ASSERT_FALSE(no_interface.has_value());
    EXPECT_EQ(no_interface.error().message, "a dbus signal needs an interface");
}

} // namespace test
