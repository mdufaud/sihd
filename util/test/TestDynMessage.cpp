#include <gtest/gtest.h>

#include <sihd/util/DynMessage.hpp>
#include <sihd/util/MessageField.hpp>

namespace test
{
using namespace sihd::util;

class TestDynMessage: public ::testing::Test
{
    protected:
        TestDynMessage() = default;
        virtual ~TestDynMessage() = default;
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

} // namespace test
