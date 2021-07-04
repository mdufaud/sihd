#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Message.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestMessage:   public ::testing::Test
    {
        protected:
            TestMessage()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestMessage()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestMessage, test_message_errors)
    {
        //TODO
    }

    TEST_F(TestMessage, test_message_waterfall)
    {
        Message msg1("msg1");

        EXPECT_TRUE(msg1.add_field<int>("int"));
        EXPECT_TRUE(msg1.add_field<char>("str", 200));
        EXPECT_TRUE(msg1.finish());

        msg1.print_tree_desc();
        EXPECT_EQ(msg1.get_field_byte_size(), sizeof(int) + 200 * sizeof(char));

        Message msg2("msg2");
        
        EXPECT_TRUE(msg2.add_field<bool>("bool"));
        EXPECT_TRUE(msg2.add_field<short>("short", 2));
        EXPECT_TRUE(msg2.add_field<double>("double", 2));
        EXPECT_TRUE(msg2.add_field("msg", msg1.clone()));
        EXPECT_TRUE(msg2.add_field("othermsg", msg1.clone()));
        EXPECT_TRUE(msg2.finish());

        msg2.print_tree_desc();
        EXPECT_EQ(msg2.get_field_byte_size(), msg1.get_field_byte_size() * 2
            + sizeof(bool) + sizeof(short) * 2 + sizeof(double) * 2);
 
    }

    TEST_F(TestMessage, test_message_simple)
    {
        Message msg("message");

        EXPECT_TRUE(msg.add_field<bool>("bool"));
        EXPECT_TRUE(msg.add_field<uint32_t>("uint", 4));
        EXPECT_TRUE(msg.add_field<float>("float", 8));
        EXPECT_TRUE(msg.finish());
        msg.print_tree();
        EXPECT_EQ(msg.get_field_byte_size(),
            sizeof(bool) + sizeof(uint32_t) * 4 + sizeof(float) * 8);

        // Making a fake buffer to fill the message
        Byte    buf;
        Bool    barr;
        UInt    iarr;
        Float   farr;

        buf.resize(1 + 4 * sizeof(int) + 8 * sizeof(float));
        barr.assign_bytes(buf.buf(), sizeof(bool));
        iarr.assign_bytes(barr.buf() + barr.byte_size(), 4 * sizeof(int));
        farr.assign_bytes(iarr.buf() + iarr.byte_size(), 8 * sizeof(float));
        
        barr[0] = true;
        for (size_t i = 0; i < iarr.size(); ++i)
            iarr[i] = 10 * i;
        for (size_t i = 0; i < farr.size(); ++i)
            farr[i] = 3.0 + (0.1 * i);

        EXPECT_EQ(buf.size(), msg.get_field_byte_size());

        // Fill the message
        msg.field_read_from(buf.buf(), buf.size());

        // Retrieve fields and check buffer positionning
        MessageField *bfield = msg.find<MessageField>("bool");
        MessageField *ifield = msg.find<MessageField>("uint");
        MessageField *ffield = msg.find<MessageField>("float");
        EXPECT_NE(bfield, nullptr);
        EXPECT_NE(ifield, nullptr);
        EXPECT_NE(ffield, nullptr);
        EXPECT_EQ(bfield->array()->buf(), msg.array()->buf());
        EXPECT_EQ(ifield->array()->buf(), msg.array()->buf() + 1);
        EXPECT_EQ(ffield->array()->buf(), msg.array()->buf() + 1 + (4 * sizeof(int)));

        // Test if values are correct
        EXPECT_EQ(bfield->read_value<bool>(0), true);
        for (size_t i = 0; i < iarr.size(); ++i)
            EXPECT_EQ(ifield->read_value<uint32_t>(i), 10u * i);
        for (size_t i = 0; i < farr.size(); ++i)
            EXPECT_FLOAT_EQ(ffield->read_value<float>(i), 3.0 + (0.1 * i));
    }
}
