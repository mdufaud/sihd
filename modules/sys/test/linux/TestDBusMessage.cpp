#include <gtest/gtest.h>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys::linux_::dbus;

class TestDBusMessage: public ::testing::Test
{
    protected:
        TestDBusMessage() { LoggerManager::stream(); }

        virtual ~TestDBusMessage() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!Connection::supported)
            {
                GTEST_SKIP() << "dbus not supported on this platform";
            }
            auto message = new_ping();
            ASSERT_TRUE(message.has_value()) << message.error().message;
            _message = std::move(*message);
        }

        virtual void TearDown() {}

        static Result<Message> new_ping()
        {
            return Message::method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
        }

        std::optional<Message> _message;
};

TEST_F(TestDBusMessage, test_append_containers)
{
    Appender body(*_message);
    ASSERT_TRUE(body.add("app").has_value());
    auto names = body.open_array("s");
    ASSERT_TRUE(names.has_value()) << names.error().message;
    ASSERT_TRUE(names->add("first").has_value());
    ASSERT_TRUE(names->add("second").has_value());
    ASSERT_TRUE(body.close(std::move(*names)).has_value());

    auto hints = body.open_array("{sv}");
    ASSERT_TRUE(hints.has_value()) << hints.error().message;
    auto entry = hints->open_dict_entry();
    ASSERT_TRUE(entry.has_value()) << entry.error().message;
    ASSERT_TRUE(entry->add("urgency").has_value());
    auto variant = entry->open_variant("y");
    ASSERT_TRUE(variant.has_value()) << variant.error().message;
    ASSERT_TRUE(variant->add(uint8_t {2}).has_value());
    ASSERT_TRUE(entry->close(std::move(*variant)).has_value());
    ASSERT_TRUE(hints->close(std::move(*entry)).has_value());
    ASSERT_TRUE(body.close(std::move(*hints)).has_value());
    ASSERT_TRUE(body.add(int32_t {-1}).has_value());

    Iter iter(*_message);
    EXPECT_EQ(iter.get<std::string>().value_or(""), "app");
    ASSERT_TRUE(iter.next());
    EXPECT_EQ(iter.get_array<std::string>().value_or(std::vector<std::string> {}),
              (std::vector<std::string> {"first", "second"}));

    ASSERT_TRUE(iter.next());
    auto read_hints = iter.recurse();
    ASSERT_TRUE(read_hints.has_value()) << read_hints.error().message;
    auto read_entry = read_hints->recurse();
    ASSERT_TRUE(read_entry.has_value()) << read_entry.error().message;
    EXPECT_EQ(read_entry->get<std::string>().value_or(""), "urgency");
    ASSERT_TRUE(read_entry->next());
    auto read_variant = read_entry->recurse();
    ASSERT_TRUE(read_variant.has_value()) << read_variant.error().message;
    EXPECT_EQ(read_variant->get<uint8_t>().value_or(0), 2);
    EXPECT_FALSE(read_hints->next());

    ASSERT_TRUE(iter.next());
    EXPECT_EQ(iter.get<int32_t>().value_or(0), -1);
    EXPECT_FALSE(iter.next());
}

TEST_F(TestDBusMessage, test_append_array_of_structs)
{
    Appender body(*_message);
    auto pairs = body.open_array("(si)");
    ASSERT_TRUE(pairs.has_value()) << pairs.error().message;
    auto pair = pairs->open_struct();
    ASSERT_TRUE(pair.has_value()) << pair.error().message;

    auto wrong_field = pair->add(int32_t {1});
    ASSERT_TRUE(pair->add("one").has_value());
    ASSERT_TRUE(pair->add(int32_t {1}).has_value());
    ASSERT_TRUE(pairs->close(std::move(*pair)).has_value());
    ASSERT_TRUE(body.close(std::move(*pairs)).has_value());

    ASSERT_FALSE(wrong_field.has_value());
    EXPECT_EQ(wrong_field.error().message, "dbus type mismatch: expected 's', got 'i'");
    Iter iter(*_message);
    auto read_pairs = iter.recurse();
    ASSERT_TRUE(read_pairs.has_value()) << read_pairs.error().message;
    auto read_pair = read_pairs->recurse();
    ASSERT_TRUE(read_pair.has_value()) << read_pair.error().message;
    EXPECT_EQ(read_pair->get<std::string>().value_or(""), "one");
    ASSERT_TRUE(read_pair->next());
    EXPECT_EQ(read_pair->get<int32_t>().value_or(0), 1);
}

TEST_F(TestDBusMessage, test_append_rejects_unexpected_types)
{
    Appender body(*_message);
    auto names = body.open_array("s");
    ASSERT_TRUE(names.has_value()) << names.error().message;

    auto wrong_element = names->add(int32_t {1});
    auto struct_in_string_array = names->open_struct();
    auto entry_outside_dict = names->open_dict_entry();
    ASSERT_TRUE(body.close(std::move(*names)).has_value());
    auto bad_array_signature = body.open_array("{s");
    auto bad_variant_signature = body.open_variant("ss");

    ASSERT_FALSE(wrong_element.has_value());
    EXPECT_EQ(wrong_element.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(wrong_element.error().message, "dbus type mismatch: expected 's', got 'i'");
    ASSERT_FALSE(struct_in_string_array.has_value());
    EXPECT_EQ(struct_in_string_array.error().message, "dbus type mismatch: expected 's', got '('");
    ASSERT_FALSE(entry_outside_dict.has_value());
    EXPECT_EQ(entry_outside_dict.error().message, "dbus type mismatch: expected 's', got '{'");
    ASSERT_FALSE(bad_array_signature.has_value());
    EXPECT_EQ(bad_array_signature.error().message, "invalid dbus array element signature: {s");
    ASSERT_FALSE(bad_variant_signature.has_value());
    EXPECT_EQ(bad_variant_signature.error().message, "invalid dbus variant signature: ss");
}

TEST_F(TestDBusMessage, test_append_dict_entry_needs_a_dict)
{
    Appender body(*_message);

    auto entry_in_body = body.open_dict_entry();

    ASSERT_FALSE(entry_in_body.has_value());
    EXPECT_EQ(entry_in_body.error().message, "a dbus dict entry only goes in an array of dict entries");
}

TEST_F(TestDBusMessage, test_append_variant_takes_one_value)
{
    Appender body(*_message);
    auto empty_variant = body.open_variant("y");
    ASSERT_TRUE(empty_variant.has_value()) << empty_variant.error().message;

    auto empty_close = body.close(std::move(*empty_variant));
    ASSERT_TRUE(empty_variant->add(uint8_t {1}).has_value());
    auto second_value = empty_variant->add(uint8_t {2});

    ASSERT_FALSE(empty_close.has_value());
    EXPECT_EQ(empty_close.error().message, "dbus container misses 'y'");
    ASSERT_FALSE(second_value.has_value());
    EXPECT_EQ(second_value.error().message, "dbus container takes no more values");
    EXPECT_TRUE(body.close(std::move(*empty_variant)).has_value());
}

TEST_F(TestDBusMessage, test_append_follows_container_order)
{
    Appender body(*_message);
    auto outer = body.open_struct();
    ASSERT_TRUE(outer.has_value()) << outer.error().message;
    auto inner = outer->open_struct();
    ASSERT_TRUE(inner.has_value()) << inner.error().message;
    auto foreign = Message::method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Pong");
    ASSERT_TRUE(foreign.has_value()) << foreign.error().message;
    Appender foreign_body(*foreign);

    auto add_under_open_child = body.add(true);
    auto close_with_open_child = body.close(std::move(*outer));
    auto close_by_stranger = foreign_body.close(std::move(*inner));

    ASSERT_FALSE(add_under_open_child.has_value());
    EXPECT_EQ(add_under_open_child.error().message, "dbus container has an open child");
    ASSERT_FALSE(close_with_open_child.has_value());
    EXPECT_EQ(close_with_open_child.error().message, "dbus container has an open child");
    ASSERT_FALSE(close_by_stranger.has_value());
    EXPECT_EQ(close_by_stranger.error().message, "dbus container was not opened by this appender");
}

TEST_F(TestDBusMessage, test_append_after_dropped_container)
{
    Appender body(*_message);
    {
        auto dropped = body.open_struct();
        ASSERT_TRUE(dropped.has_value()) << dropped.error().message;
    }

    auto added = body.add(true);

    ASSERT_FALSE(added.has_value());
    EXPECT_EQ(added.error().message, "dbus message has an abandoned container");
}

TEST_F(TestDBusMessage, test_empty_message)
{
    Message kept = std::move(*_message);
    Appender body(*_message);

    auto appended = body.add(true);
    auto added = _message->add(true);

    ASSERT_FALSE(appended.has_value());
    EXPECT_EQ(appended.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(appended.error().message, "dbus message is empty");
    ASSERT_FALSE(added.has_value());
    EXPECT_EQ(added.error().message, "dbus message is empty");
}

TEST_F(TestDBusMessage, test_reply_needs_received_call)
{
    auto signal = Message::signal("/org/sihd/Test", "org.sihd.Test", "Hello");
    ASSERT_TRUE(signal.has_value()) << signal.error().message;

    auto unsent_reply = _message->method_return();
    auto unsent_error = _message->error_return("org.sihd.Error.Failed", "failed");
    auto signal_reply = signal->method_return();

    ASSERT_FALSE(unsent_reply.has_value());
    EXPECT_EQ(unsent_reply.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(unsent_reply.error().message, "a dbus reply answers a received method call");
    ASSERT_FALSE(unsent_error.has_value());
    EXPECT_EQ(unsent_error.error().message, "a dbus reply answers a received method call");
    ASSERT_FALSE(signal_reply.has_value());
    EXPECT_EQ(signal_reply.error().message, "a dbus reply answers a method call");
}

TEST_F(TestDBusMessage, test_open_bad_address)
{
    Connection connection;

    auto opened = connection.open("nonsense");

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::io_error);
    EXPECT_TRUE(opened.error().message.starts_with("org.freedesktop.DBus.Error.BadAddress: "))
        << opened.error().message;
    EXPECT_FALSE(connection.is_open());
}

} // namespace test
