#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

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
            auto built = _message.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
            ASSERT_TRUE(built.has_value()) << built.error().message;
        }

        virtual void TearDown() {}

        Message _message;
};

TEST_F(TestDBusMessage, test_append_containers)
{
    Appender body(_message);
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

    Iter iter(_message);
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
    Appender body(_message);
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
    Iter iter(_message);
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
    Appender body(_message);
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
    Appender body(_message);

    auto entry_in_body = body.open_dict_entry();

    ASSERT_FALSE(entry_in_body.has_value());
    EXPECT_EQ(entry_in_body.error().message, "a dbus dict entry only goes in an array of dict entries");
}

TEST_F(TestDBusMessage, test_append_variant_takes_one_value)
{
    Appender body(_message);
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
    Appender body(_message);
    auto outer = body.open_struct();
    ASSERT_TRUE(outer.has_value()) << outer.error().message;
    auto inner = outer->open_struct();
    ASSERT_TRUE(inner.has_value()) << inner.error().message;
    Message foreign;
    auto built = foreign.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Pong");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    Appender foreign_body(foreign);

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

TEST_F(TestDBusMessage, test_append_refuses_an_empty_struct)
{
    Appender body(_message);
    auto record = body.open_struct();
    ASSERT_TRUE(record.has_value()) << record.error().message;

    auto empty_close = body.close(std::move(*record));
    ASSERT_TRUE(record->add(true).has_value());
    auto filled_close = body.close(std::move(*record));

    ASSERT_FALSE(empty_close.has_value());
    EXPECT_EQ(empty_close.error().message, "dbus struct has no field");
    EXPECT_TRUE(filled_close.has_value()) << filled_close.error().message;
}

TEST_F(TestDBusMessage, test_append_struct_nesting_is_bounded)
{
    Appender body(_message);
    std::vector<Appender> structs;
    structs.reserve(32);
    Appender *parent = &body;
    for (int depth = 0; depth < 32; ++depth)
    {
        auto opened = parent->open_struct();
        ASSERT_TRUE(opened.has_value()) << depth << ": " << opened.error().message;
        structs.push_back(std::move(*opened));
        parent = &structs.back();
    }

    auto too_deep = parent->open_struct();

    ASSERT_FALSE(too_deep.has_value());
    EXPECT_EQ(too_deep.error().message, "dbus structs nest at most 32 deep");
}

TEST_F(TestDBusMessage, test_append_after_dropped_container)
{
    Appender body(_message);
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
    Message kept = std::move(_message);
    Appender body(_message);

    auto appended = body.add(true);
    auto added = _message.add(true);

    ASSERT_FALSE(appended.has_value());
    EXPECT_EQ(appended.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(appended.error().message, "dbus message is empty");
    ASSERT_FALSE(added.has_value());
    EXPECT_EQ(added.error().message, "dbus message is empty");
    EXPECT_EQ(kept.member(), "Ping");
}

TEST_F(TestDBusMessage, test_default_message_is_empty)
{
    Message empty;

    EXPECT_EQ(empty.path(), "");
    EXPECT_EQ(empty.interface(), "");
    EXPECT_EQ(empty.member(), "");
    EXPECT_EQ(empty.sender(), "");
    EXPECT_EQ(empty.destination(), "");
    EXPECT_EQ(empty.error_name(), "");
    EXPECT_EQ(empty.signature(), "");
    EXPECT_EQ(empty.type(), Message::Type::invalid);
    EXPECT_EQ(empty.serial(), 0u);
    EXPECT_EQ(empty.reply_serial(), 0u);
    EXPECT_FALSE(empty.expects_reply());
    EXPECT_EQ(empty.to_error().code, ErrorCode::not_initialized);
    EXPECT_EQ(empty.to_error().message, "dbus message is empty");
}

TEST_F(TestDBusMessage, test_empty_message_refuses_writes)
{
    Message empty;

    auto no_reply_set = empty.set_no_reply();
    auto added = empty.add(true);
    auto text_added = empty.add("text");
    auto path_added = empty.add_object_path("/org/sihd/Test");
    auto signature_added = empty.add_signature("s");
    auto fd_added = empty.add_unix_fd(0);

    for (const auto & refused : {no_reply_set, added, text_added, path_added, signature_added, fd_added})
    {
        ASSERT_FALSE(refused.has_value());
        EXPECT_EQ(refused.error().code, ErrorCode::not_initialized);
        EXPECT_EQ(refused.error().message, "dbus message is empty");
    }
}

TEST_F(TestDBusMessage, test_empty_message_refuses_to_answer_nor_read_changes)
{
    Message empty;

    auto returned = empty.method_return();
    auto failed = empty.error_return("org.sihd.Error.Failed", "failed");
    auto changed = empty.changed_property("org.sihd.Test", "Value");

    ASSERT_FALSE(returned.has_value());
    EXPECT_EQ(returned.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(returned.error().message, "dbus message is empty");
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error().message, "dbus message is empty");
    ASSERT_FALSE(changed.has_value());
    EXPECT_EQ(changed.error().message, "dbus message is empty");
}

TEST_F(TestDBusMessage, test_appender_on_an_empty_message_refuses)
{
    Message empty;
    Appender body(empty);

    auto appended = body.add(true);
    auto opened = body.open_array("s");

    ASSERT_FALSE(appended.has_value());
    EXPECT_EQ(appended.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(appended.error().message, "dbus message is empty");
    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().message, "dbus message is empty");
}

TEST_F(TestDBusMessage, test_iter_on_an_empty_message_reads_nothing)
{
    Message empty;
    Iter iter(empty);

    auto read = iter.get<bool>();
    auto text = iter.get<std::string>();
    auto array = iter.get_array<uint8_t>();
    auto fd = iter.get_unix_fd();
    auto child = iter.recurse();

    EXPECT_EQ(iter.type(), '\0');
    EXPECT_EQ(iter.signature(), "");
    EXPECT_FALSE(iter.next());
    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().message, fmt::format("dbus type mismatch: expected 'b', got '{}'", '\0'));
    ASSERT_FALSE(text.has_value());
    EXPECT_EQ(text.error().message, fmt::format("dbus type mismatch: expected 's', got '{}'", '\0'));
    ASSERT_FALSE(array.has_value());
    ASSERT_FALSE(fd.has_value());
    ASSERT_FALSE(child.has_value());
    EXPECT_EQ(child.error().message, fmt::format("dbus type '{}' is not a container", '\0'));
}

TEST_F(TestDBusMessage, test_failed_builder_keeps_the_message)
{
    auto bad_path = _message.method_call("org.sihd.Test", "no slash", "org.sihd.Test", "Pong");
    auto bad_signal = _message.signal("/org/sihd/Test", "", "Hello");
    auto bad_properties = _message.properties_get("org.sihd.Test", "no slash", "org.sihd.Test", "Value");

    ASSERT_FALSE(bad_path.has_value());
    EXPECT_EQ(bad_path.error().message, "invalid dbus path: no slash");
    ASSERT_FALSE(bad_signal.has_value());
    EXPECT_EQ(bad_signal.error().message, "a dbus signal needs an interface");
    ASSERT_FALSE(bad_properties.has_value());
    EXPECT_EQ(bad_properties.error().message, "invalid dbus path: no slash");
    EXPECT_EQ(_message.type(), Message::Type::method_call);
    EXPECT_EQ(_message.member(), "Ping");
}

TEST_F(TestDBusMessage, test_builder_replaces_the_message)
{
    ASSERT_TRUE(_message.add(true).has_value());

    auto built = _message.signal("/org/sihd/Test", "org.sihd.Test", "Hello");

    ASSERT_TRUE(built.has_value()) << built.error().message;
    EXPECT_EQ(_message.type(), Message::Type::signal);
    EXPECT_EQ(_message.member(), "Hello");
    EXPECT_EQ(_message.signature(), "");
}

TEST_F(TestDBusMessage, test_properties_builders_keep_the_arguments)
{
    Message get;
    Message get_all;

    auto built_get = get.properties_get("org.sihd.Test", "/org/sihd/Test", "org.sihd.Interface", "Value");
    auto built_all = get_all.properties_get_all("org.sihd.Test", "/org/sihd/Test", "org.sihd.Interface");

    ASSERT_TRUE(built_get.has_value()) << built_get.error().message;
    EXPECT_EQ(get.member(), "Get");
    EXPECT_EQ(get.interface(), "org.freedesktop.DBus.Properties");
    EXPECT_EQ(get.signature(), "ss");
    ASSERT_TRUE(built_all.has_value()) << built_all.error().message;
    EXPECT_EQ(get_all.member(), "GetAll");
    EXPECT_EQ(get_all.signature(), "s");
}

TEST_F(TestDBusMessage, test_moved_from_message_is_empty)
{
    Message other = std::move(_message);

    EXPECT_EQ(other.member(), "Ping");
    EXPECT_EQ(_message.type(), Message::Type::invalid);
    EXPECT_EQ(_message.member(), "");

    _message = std::move(other);

    EXPECT_EQ(_message.member(), "Ping");
    EXPECT_EQ(other.type(), Message::Type::invalid);
}

TEST_F(TestDBusMessage, test_reply_needs_received_call)
{
    Message signal;
    auto built = signal.signal("/org/sihd/Test", "org.sihd.Test", "Hello");
    ASSERT_TRUE(built.has_value()) << built.error().message;

    auto unsent_reply = _message.method_return();
    auto unsent_error = _message.error_return("org.sihd.Error.Failed", "failed");
    auto signal_reply = signal.method_return();

    ASSERT_FALSE(unsent_reply.has_value());
    EXPECT_EQ(unsent_reply.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(unsent_reply.error().message, "a dbus reply answers a received method call");
    ASSERT_FALSE(unsent_error.has_value());
    EXPECT_EQ(unsent_error.error().message, "a dbus reply answers a received method call");
    ASSERT_FALSE(signal_reply.has_value());
    EXPECT_EQ(signal_reply.error().message, "a dbus reply answers a method call");
}

TEST_F(TestDBusMessage, test_wide_types)
{
    ASSERT_TRUE(_message.add(int64_t {-5000000000}).has_value());
    ASSERT_TRUE(_message.add(uint64_t {18000000000000000000u}).has_value());
    ASSERT_TRUE(_message.add(2.5).has_value());
    ASSERT_TRUE(Appender(_message).add_array(std::vector<double> {1.5, -0.25}).has_value());

    Iter body(_message);

    EXPECT_EQ(_message.signature(), "xtdad");
    EXPECT_EQ(body.get<int64_t>().value_or(0), -5000000000);
    ASSERT_TRUE(body.next());
    EXPECT_EQ(body.get<uint64_t>().value_or(0), 18000000000000000000u);
    ASSERT_TRUE(body.next());
    EXPECT_EQ(body.get<double>().value_or(0.0), 2.5);
    ASSERT_TRUE(body.next());
    EXPECT_EQ(body.get_array<double>().value_or(std::vector<double> {}), (std::vector<double> {1.5, -0.25}));
}

TEST_F(TestDBusMessage, test_signature_argument)
{
    auto invalid = _message.add_signature("a");
    ASSERT_TRUE(_message.add_signature("a{sv}").has_value());

    Iter body(_message);

    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(invalid.error().message, "invalid dbus signature: a");
    EXPECT_EQ(body.type(), 'g');
    EXPECT_EQ(body.get<std::string>().value_or(""), "a{sv}");
}

TEST_F(TestDBusMessage, test_byte_maps_and_empty_vardict)
{
    // 0x004c: Apple company id, 0x02 0x15: iBeacon type and length, 180f: battery service, 0x64: 100%
    const std::map<uint16_t, std::vector<uint8_t>> manufacturer_data {{0x004c, {0x02, 0x15}}};
    const std::map<std::string, std::vector<uint8_t>> service_data {{"180f", {0x64}}};
    Appender body(_message);

    auto manufacturer_added = body.add_byte_map(manufacturer_data);
    auto service_added = body.add_byte_map(service_data);
    auto options_added = body.add_empty_vardict();

    ASSERT_TRUE(manufacturer_added.has_value()) << manufacturer_added.error().message;
    ASSERT_TRUE(service_added.has_value()) << service_added.error().message;
    ASSERT_TRUE(options_added.has_value()) << options_added.error().message;
    EXPECT_EQ(_message.signature(), "a{qv}a{sv}a{sv}");
    Iter reader(_message);
    EXPECT_EQ(reader.get_byte_map<uint16_t>().value_or(std::map<uint16_t, std::vector<uint8_t>> {}), manufacturer_data);
    ASSERT_TRUE(reader.next());
    EXPECT_EQ(reader.get_byte_map<std::string>().value_or(std::map<std::string, std::vector<uint8_t>> {}),
              service_data);
    ASSERT_TRUE(reader.next());
    auto options = reader.get_byte_map<std::string>();
    ASSERT_TRUE(options.has_value()) << options.error().message;
    EXPECT_TRUE(options->empty());
}

TEST_F(TestDBusMessage, test_iter_signature)
{
    Appender body(_message);
    auto variant = body.open_variant("as");
    ASSERT_TRUE(variant.has_value()) << variant.error().message;
    ASSERT_TRUE(variant->add_array(std::vector<std::string> {"one"}).has_value());
    ASSERT_TRUE(body.close(std::move(*variant)).has_value());

    Iter reader(_message);
    auto value = reader.recurse();
    ASSERT_TRUE(value.has_value()) << value.error().message;

    EXPECT_EQ(reader.signature(), "v");
    EXPECT_EQ(value->signature(), "as");
    EXPECT_FALSE(reader.next());
    EXPECT_EQ(reader.signature(), "");
}

TEST_F(TestDBusMessage, test_call_metadata)
{
    EXPECT_EQ(_message.destination(), "org.sihd.Test");
    EXPECT_EQ(_message.signature(), "");
    EXPECT_TRUE(_message.expects_reply());

    ASSERT_TRUE(_message.set_no_reply().has_value());

    EXPECT_FALSE(_message.expects_reply());
}

TEST_F(TestDBusMessage, test_signal_expects_no_reply)
{
    Message signal;
    auto built = signal.signal("/org/sihd/Test", "org.sihd.Test", "Changed");
    ASSERT_TRUE(built.has_value()) << built.error().message;

    EXPECT_FALSE(signal.expects_reply());
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
