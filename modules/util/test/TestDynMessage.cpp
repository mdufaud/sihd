#include <gtest/gtest.h>

#include <sihd/util/DynMessage.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/MessageField.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;

class TestDynMessage: public ::testing::Test
{
    protected:
        TestDynMessage() { sihd::util::LoggerManager::stream(); }
        virtual ~TestDynMessage() { sihd::util::LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestDynMessage, test_dynmessage_read_write)
{
    DynMessage msg("test");

    EXPECT_TRUE(msg.add_field<int>("x"));
    EXPECT_TRUE(msg.add_field<float>("y"));
    EXPECT_TRUE(msg.finish());

    struct __attribute__((packed))
    {
            int x;
            float y;
    } data = {42, 3.14f};

    EXPECT_TRUE(msg.field_read_from(&data, sizeof(data)));
    EXPECT_EQ(msg.field_byte_size(), sizeof(data));

    decltype(data) out = {};
    EXPECT_TRUE(msg.field_write_to(&out, sizeof(out)));
    EXPECT_EQ(out.x, 42);
    EXPECT_FLOAT_EQ(out.y, 3.14f);
}

TEST_F(TestDynMessage, test_dynmessage_hide_field)
{
    DynMessage msg("test");

    EXPECT_TRUE(msg.add_field<int>("a"));
    EXPECT_TRUE(msg.add_field<int>("b"));
    EXPECT_TRUE(msg.finish());

    struct __attribute__((packed))
    {
            int a;
            int b;
    } full = {1, 2};

    EXPECT_TRUE(msg.field_read_from(&full, sizeof(full)));

    EXPECT_TRUE(msg.hide_field("b", true));

    int partial = 0;
    EXPECT_TRUE(msg.field_write_to(&partial, sizeof(partial)));
    EXPECT_EQ(partial, 1);

    EXPECT_TRUE(msg.hide_field("b", false));
}

TEST_F(TestDynMessage, test_dynmessage_clone)
{
    DynMessage msg("test");
    EXPECT_TRUE(msg.add_field<int>("val"));
    EXPECT_TRUE(msg.finish());

    IMessageField *cloned = msg.clone();
    ASSERT_NE(cloned, nullptr);

    int data = 99;
    EXPECT_TRUE(cloned->field_read_from(&data, sizeof(data)));
    EXPECT_EQ(cloned->field_byte_size(), sizeof(int));

    delete cloned;
}

TEST_F(TestDynMessage, test_dynmessage_rule_callback)
{
    DynMessage msg("test");
    EXPECT_TRUE(msg.add_field<int>("val"));
    EXPECT_TRUE(msg.finish());

    int callback_count = 0;
    EXPECT_TRUE(msg.add_rule("val", [&callback_count](DynMessage &, const IMessageField &) { ++callback_count; }));

    int data = 42;
    EXPECT_TRUE(msg.field_read_from(&data, sizeof(data)));
    EXPECT_EQ(callback_count, 1);

    data = 99;
    EXPECT_TRUE(msg.field_read_from(&data, sizeof(data)));
    EXPECT_EQ(callback_count, 2);
}

// Regression: _total_dyn_size was not reset to 0 at the start of field_read_from,
// so a failed read left the stale size from the previous successful read.
TEST_F(TestDynMessage, test_dynmessage_total_size_resets_on_failed_read)
{
    DynMessage msg("test");
    EXPECT_TRUE(msg.add_field<int>("val"));
    EXPECT_TRUE(msg.finish());

    int data = 42;
    EXPECT_TRUE(msg.field_read_from(&data, sizeof(data)));
    EXPECT_EQ(msg.field_byte_size(), sizeof(int));

    uint8_t tiny = 0;
    EXPECT_FALSE(msg.field_read_from(&tiny, sizeof(tiny)));
    EXPECT_EQ(msg.field_byte_size(), 0u);
}

// Regression: clone() did not copy _hidden_fields, so hidden fields were visible again in the clone.
TEST_F(TestDynMessage, test_dynmessage_clone_preserves_hidden_fields)
{
    DynMessage msg("test");
    EXPECT_TRUE(msg.add_field<int>("a"));
    EXPECT_TRUE(msg.add_field<int>("b"));
    EXPECT_TRUE(msg.finish());
    EXPECT_TRUE(msg.hide_field("b", true));

    DynMessage *cloned = dynamic_cast<DynMessage *>(msg.clone());
    ASSERT_NE(cloned, nullptr);

    struct __attribute__((packed))
    {
            int a;
            int b;
    } data = {7, 99};

    EXPECT_TRUE(cloned->field_read_from(&data, sizeof(data)));
    // "b" is hidden in the clone: only "a" should be consumed
    EXPECT_EQ(cloned->field_byte_size(), sizeof(int));
    EXPECT_EQ(cloned->find<MessageField>("a")->read_value<int>(0), 7);

    delete cloned;
}

// Verifies that rule IDs are stable: removing rule A does not invalidate rule B's ID.
TEST_F(TestDynMessage, test_dynmessage_rule_stable_ids)
{
    DynMessage msg("test");
    EXPECT_TRUE(msg.add_field<int>("val"));
    EXPECT_TRUE(msg.finish());

    int count_a = 0;
    int count_b = 0;
    const auto id_a = msg.add_rule("val", [&count_a](DynMessage &, const IMessageField &) { ++count_a; });
    const auto id_b = msg.add_rule("val", [&count_b](DynMessage &, const IMessageField &) { ++count_b; });
    ASSERT_TRUE(id_a.has_value());
    ASSERT_TRUE(id_b.has_value());

    int data = 1;
    EXPECT_TRUE(msg.field_read_from(&data, sizeof(data)));
    EXPECT_EQ(count_a, 1);
    EXPECT_EQ(count_b, 1);

    EXPECT_TRUE(msg.remove_rule("val", *id_a));

    EXPECT_TRUE(msg.field_read_from(&data, sizeof(data)));
    EXPECT_EQ(count_a, 1); // removed — no new increment
    EXPECT_EQ(count_b, 2); // still active via its original ID

    // id_b is still valid despite id_a having been removed
    EXPECT_TRUE(msg.remove_rule("val", *id_b));
    EXPECT_FALSE(msg.remove_rule("val", *id_b)); // already gone
}

} // namespace test
