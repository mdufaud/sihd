#include <gtest/gtest.h>

#include <sihd/util/DynMessage.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Message.hpp>
#include <sihd/util/MessageField.hpp>
#include <sihd/util/array_utils.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestMessage: public ::testing::Test
{
    protected:
        TestMessage() { sihd::util::LoggerManager::basic(); }

        virtual ~TestMessage() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestMessage, test_message_errors)
{
    // TODO
}

TEST_F(TestMessage, test_dynmessage)
{
    struct s_msg1
    {
            bool active;
            int size;
    } __attribute__((packed));

    DynMessage msg1("msg1");

    EXPECT_TRUE(msg1.add_field<bool>("active"));
    EXPECT_TRUE(msg1.add_field<int>("size"));
    EXPECT_TRUE(msg1.add_field<char>("str", 5));

    EXPECT_TRUE(msg1.add_rule("active", [](DynMessage & msg, const IMessageField & ifield) {
        const MessageField & field = dynamic_cast<const MessageField &>(ifield);
        msg.hide_field("str", !field.read_value<bool>(0));
    }));

    EXPECT_TRUE(msg1.add_rule("size", [](DynMessage & msg, const IMessageField & ifield) {
        const MessageField & field = dynamic_cast<const MessageField &>(ifield);
        msg.find<MessageField>("str")->field_resize(field.read_value<int>(0));
    }));

    EXPECT_TRUE(msg1.finish());

    SIHD_COUT(msg1.tree_desc_str());

    s_msg1 test;
    test.active = false;
    test.size = 0;

    MessageField *activefield = msg1.find<MessageField>("active");
    MessageField *sizefield = msg1.find<MessageField>("size");
    MessageField *strfield = msg1.find<MessageField>("str");

    ASSERT_NE(activefield, nullptr);
    ASSERT_NE(sizefield, nullptr);
    ASSERT_NE(strfield, nullptr);

    ASSERT_EQ(msg1.field_byte_size(), 0u);

    EXPECT_TRUE(msg1.field_read_from(&test, sizeof(test)));
    EXPECT_EQ(activefield->read_value<bool>(0), false);

    ASSERT_EQ(msg1.field_byte_size(), sizeof(bool) + sizeof(int));

    char hw[] = "hello world";
    char buffer[sizeof(s_msg1) + sizeof(hw)];
    s_msg1 *test2 = (s_msg1 *)&buffer[0];
    test2->active = true;
    test2->size = sizeof(hw);
    strcpy(buffer + sizeof(s_msg1), hw);

    EXPECT_TRUE(msg1.field_read_from(&buffer, sizeof(buffer)));

    ASSERT_EQ(msg1.field_byte_size(), sizeof(buffer));

    SIHD_COUT(msg1.tree_desc_str());

    EXPECT_EQ(activefield->read_value<bool>(0), true);
    EXPECT_EQ(sizefield->read_value<int>(0), (int)sizeof(hw));
    EXPECT_TRUE(strfield->array()->is_bytes_equal(hw, sizeof(hw)));
}

TEST_F(TestMessage, test_message_waterfall)
{
    struct s_msg1
    {
            int magic;
            char str[200];
    } __attribute__((packed));

    Message msg1("msg1");

    EXPECT_TRUE(msg1.add_field<int>("magic"));
    EXPECT_TRUE(msg1.add_field<char>("str", 200));
    EXPECT_TRUE(msg1.finish());

    SIHD_COUT(msg1.tree_desc_str());

    EXPECT_EQ(msg1.field_byte_size(), sizeof(int) + 200 * sizeof(char));
    EXPECT_EQ(msg1.field_byte_size(), sizeof(s_msg1));

    struct s_msg2
    {
            bool b;
            double d[2];
            short s;
            struct s_msg1 msg;
            struct s_msg1 othermsg;
    } __attribute__((packed));

    Message msg2("msg2");

    EXPECT_TRUE(msg2.add_field<bool>("b"));
    EXPECT_TRUE(msg2.add_field<double>("d", 2));
    EXPECT_TRUE(msg2.add_field<short>("s"));
    EXPECT_TRUE(msg2.add_field("msg", msg1.clone()));
    EXPECT_TRUE(msg2.add_field("othermsg", msg1.clone()));
    EXPECT_TRUE(msg2.finish());

    SIHD_COUT(msg2.tree_desc_str());

    EXPECT_EQ(msg2.field_byte_size(), msg1.field_byte_size() * 2 + sizeof(bool) + sizeof(double) * 2 + sizeof(short));

    EXPECT_EQ(msg2.field_byte_size(), sizeof(s_msg2));

    s_msg2 test;
    test.b = true;
    test.d[0] = 1.0;
    test.d[1] = 2.0;
    test.s = 20;
    test.msg.magic = 0xcafe;
    memset(test.msg.str, 0, sizeof(test.msg.str));
    strcpy(test.msg.str, "hello world");
    test.othermsg.magic = 0xefac;
    memset(test.othermsg.str, 0, sizeof(test.othermsg.str));
    strcpy(test.othermsg.str, "toto");

    EXPECT_TRUE(msg2.field_read_from(&test, sizeof(test)));

    MessageField *bfield = msg2.find<MessageField>("b");
    MessageField *dfield = msg2.find<MessageField>("d");
    MessageField *sfield = msg2.find<MessageField>("s");
    Message *msgfield = msg2.find<Message>("msg");
    Message *othermsgfield = msg2.find<Message>("othermsg");

    ASSERT_NE(bfield, nullptr);
    ASSERT_NE(dfield, nullptr);
    ASSERT_NE(sfield, nullptr);
    ASSERT_NE(msgfield, nullptr);
    ASSERT_NE(othermsgfield, nullptr);

    EXPECT_EQ(bfield->read_value<bool>(0), true);
    EXPECT_EQ(dfield->read_value<double>(0), 1.0);
    EXPECT_EQ(dfield->read_value<double>(1), 2.0);
    EXPECT_EQ(sfield->read_value<short>(0), 20);

    MessageField *magicfield = msgfield->find<MessageField>("magic");
    MessageField *strfield = msgfield->find<MessageField>("str");
    ASSERT_NE(magicfield, nullptr);
    ASSERT_NE(strfield, nullptr);

    EXPECT_EQ(magicfield->read_value<int>(0), 0xcafe);
    EXPECT_TRUE(strfield->array()->is_bytes_equal("hello world", sizeof("hello world")));

    magicfield = othermsgfield->find<MessageField>("magic");
    strfield = othermsgfield->find<MessageField>("str");
    ASSERT_NE(magicfield, nullptr);
    ASSERT_NE(strfield, nullptr);

    EXPECT_EQ(magicfield->read_value<int>(0), 0xefac);
    EXPECT_TRUE(strfield->array()->is_bytes_equal("toto", sizeof("toto")));
}

TEST_F(TestMessage, test_message_simple)
{
    Message msg("message");

    EXPECT_TRUE(msg.add_field<bool>("bool"));
    EXPECT_TRUE(msg.add_field<uint32_t>("uint", 2));
    EXPECT_TRUE(msg.add_field<float>("float", 5));
    EXPECT_TRUE(msg.finish());

    SIHD_COUT(msg.tree_str());

    EXPECT_EQ(msg.field_byte_size(), sizeof(bool) + sizeof(uint32_t) * 2 + sizeof(float) * 5);

    // Making a fake buffer to fill the message
    ArrByte buf;
    ArrBool barr;
    ArrUInt iarr;
    ArrFloat farr;

    buf.resize(1 + 2 * sizeof(int) + 5 * sizeof(float));
    EXPECT_TRUE(array_utils::distribute_array(buf, {{&barr, 1}, {&iarr, 2}, {&farr, 5}}));
    barr[0] = true;
    for (size_t i = 0; i < iarr.size(); ++i)
        iarr[i] = 10 * i;
    for (size_t i = 0; i < farr.size(); ++i)
        farr[i] = 3.0 + (0.1 * i);

    EXPECT_EQ(buf.size(), msg.field_byte_size());

    // Fill the message
    ASSERT_TRUE(msg.field_read_from(buf.buf(), buf.size()));

    // Retrieve fields and check buffer positionning
    MessageField *bfield = msg.find<MessageField>("bool");
    MessageField *ifield = msg.find<MessageField>("uint");
    MessageField *ffield = msg.find<MessageField>("float");
    ASSERT_NE(bfield, nullptr);
    ASSERT_NE(ifield, nullptr);
    ASSERT_NE(ffield, nullptr);
    EXPECT_EQ(bfield->array()->buf(), msg.array()->buf());
    EXPECT_EQ(ifield->array()->buf(), msg.array()->buf() + 1);
    EXPECT_EQ(ffield->array()->buf(), msg.array()->buf() + 1 + (2 * sizeof(int)));

    // Test if values are correct
    EXPECT_EQ(bfield->read_value<bool>(0), true);
    for (size_t i = 0; i < iarr.size(); ++i)
        EXPECT_EQ(ifield->read_value<uint32_t>(i), 10u * i);
    for (size_t i = 0; i < farr.size(); ++i)
        EXPECT_FLOAT_EQ(ffield->read_value<float>(i), 3.0 + (0.1 * i));
}
} // namespace test
