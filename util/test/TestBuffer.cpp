#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Buffer.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestBuffer:   public ::testing::Test
    {
        protected:
            TestBuffer()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestBuffer()
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

    TEST_F(TestBuffer, test_buffer_store)
    {
        sihd::util::Byte buffer_byte(200);
        buffer_byte[0] = 127;
        buffer_byte[1] = 25;
        IBuffer *buffer = &buffer_byte;
        EXPECT_EQ(buffer->data()[0], 127);
        EXPECT_EQ(buffer->data()[1], 25u);

        sihd::util::Int buffer_int(2);
        buffer_int[0] = 123;
        buffer_int[1] = 456;
        buffer = &buffer_int;
        EXPECT_EQ(((int *)buffer->data())[0], 123);
        EXPECT_EQ(((int *)buffer->data())[1], 456);
    }

    TEST_F(TestBuffer, test_buffer_all)
    {
        sihd::util::Byte buffer_byte;
        buffer_byte[0] = 127;
        EXPECT_EQ(buffer_byte[0], 127);
        sihd::util::UByte buffer_ubyte;
        buffer_ubyte[0] = 255;
        EXPECT_EQ(buffer_ubyte[0], 255);

        EXPECT_EQ(buffer_byte.data_size(), sizeof(char));
        EXPECT_EQ(buffer_byte.data_type(), Datatypes::BYTE);
        EXPECT_EQ(buffer_ubyte.data_size(), sizeof(char));
        EXPECT_EQ(buffer_ubyte.data_type(), Datatypes::UBYTE);

        sihd::util::Short buffer_short;
        buffer_short[0] = 2;
        EXPECT_EQ(buffer_short[0], 2);
        sihd::util::UShort buffer_ushort;
        buffer_ushort[0] = 2;
        EXPECT_EQ(buffer_ushort[0], 2u);

        EXPECT_EQ(buffer_short.data_size(), sizeof(short));
        EXPECT_EQ(buffer_short.data_type(), Datatypes::SHORT);
        EXPECT_EQ(buffer_ushort.data_size(), sizeof(short));
        EXPECT_EQ(buffer_ushort.data_type(), Datatypes::USHORT);

        sihd::util::Int buffer_int;
        buffer_int[0] = 2;
        EXPECT_EQ(buffer_int[0], 2);
        sihd::util::UInt buffer_uint;
        buffer_uint[0] = -1;
        EXPECT_EQ(buffer_uint[0], -1u);

        EXPECT_EQ(buffer_int.data_size(), sizeof(int));
        EXPECT_EQ(buffer_int.data_type(), Datatypes::INT);
        EXPECT_EQ(buffer_uint.data_size(), sizeof(int));
        EXPECT_EQ(buffer_uint.data_type(), Datatypes::UINT);

        sihd::util::Long buffer_long;
        buffer_long[0] = 1;
        EXPECT_EQ(buffer_long[0], 1l);
        sihd::util::ULong buffer_ulong;
        buffer_ulong[0] = -1;
        EXPECT_EQ(buffer_ulong[0], -1ul);

        EXPECT_EQ(buffer_long.data_size(), sizeof(long));
        EXPECT_EQ(buffer_long.data_type(), Datatypes::LONG);
        EXPECT_EQ(buffer_ulong.data_size(), sizeof(long));
        EXPECT_EQ(buffer_ulong.data_type(), Datatypes::ULONG);

        sihd::util::Float buffer_float;
        buffer_float[0] = 133.7;
        EXPECT_FLOAT_EQ(buffer_float[0], 133.7f);

        EXPECT_EQ(buffer_float.data_size(), sizeof(float));
        EXPECT_EQ(buffer_float.data_type(), Datatypes::FLOAT);

        sihd::util::Double buffer_dbl;
        buffer_dbl[0] = 123.4;
        EXPECT_FLOAT_EQ(buffer_dbl[0], 123.4);

        EXPECT_EQ(buffer_dbl.data_size(), sizeof(double));
        EXPECT_EQ(buffer_dbl.data_type(), Datatypes::DOUBLE);
    }

    TEST_F(TestBuffer, test_buffer_err)
    {
        sihd::util::Int buffer(2);

        buffer[0] = 1;
        buffer[1] = 2;
        EXPECT_THROW(buffer[2], std::out_of_range);
        EXPECT_THROW(buffer[2] = 20, std::out_of_range);
    }

    TEST_F(TestBuffer, test_buffer_copy)
    {
        sihd::util::Int buffer(2);

        buffer[0] = 13;
        buffer[1] = 37;
        EXPECT_EQ(buffer[0], 13);
        EXPECT_EQ(buffer[1], 37);

        sihd::util::Int buffer_copied;
        EXPECT_EQ(buffer_copied.from(buffer), true);
        EXPECT_EQ(buffer_copied[0], 13);
        EXPECT_EQ(buffer_copied[1], 37);

        sihd::util::Float buffer_impossible_copy;
        EXPECT_EQ(buffer_impossible_copy.from(buffer), false);

        std::unique_ptr<Int> buffer_clone = buffer.clone();
        EXPECT_EQ(buffer_clone.get()->at(0), 13);
        EXPECT_EQ(buffer_clone.get()->at(1), 37);
    }

    TEST_F(TestBuffer, test_buffer_assign)
    {
        int ptr_int[2];
        ptr_int[0] = 123;
        ptr_int[1] = 456;
        sihd::util::Int buffer;
        buffer.assign((unsigned char *)ptr_int, 2 * sizeof(int));
        EXPECT_EQ(buffer[0], 123);
        EXPECT_EQ(buffer[1], 456);
    }
}
