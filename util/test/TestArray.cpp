#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Array.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestArray:   public ::testing::Test
    {
        protected:
            TestArray()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestArray()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            void    test_array(Array<uint8_t> & arr)
            {
                size_t i = 0;
                while (i < arr.size())
                {
                    int j = i % 4;
                    EXPECT_EQ(arr[i], j + 1);
                    ++i;
                }
            }
    };

    /*
    Not fully functionnal, anything that reallocate and memcpy will fail
    TEST_F(TestArray, test_array_str)
    {
        Array<std::string> arr;
        arr.resize(2);
        arr[0] = "hello world";
        arr[1] = "how do you do ?";
        EXPECT_EQ(arr[0], "hello world");
        EXPECT_EQ(arr.at(1), "how do you do ?");
        arr.data()->append(" !");
        EXPECT_EQ(arr[0], "hello world !");
    }
    */

    TEST_F(TestArray, test_array_pop)
    {
        Array<double> arr;
        arr.resize(3);
        arr[0] = 10.0;
        arr[1] = 20.0;
        arr[2] = 30.0;
        EXPECT_EQ(arr.size(), 3ul);
        EXPECT_EQ(arr.capacity(), 3ul);

        EXPECT_EQ(arr.front(), 10.0);
        EXPECT_EQ(arr.back(), 30.0);

        double popped = arr.pop(1);
        EXPECT_EQ(arr.size(), 2ul);
        EXPECT_EQ(arr.capacity(), 3ul);
        EXPECT_FLOAT_EQ(popped, 20.0);
        EXPECT_FLOAT_EQ(arr[0], 10.0);
        EXPECT_FLOAT_EQ(arr[1], 30.0);
        EXPECT_FLOAT_EQ(arr[2], 0.0);

        popped = arr.pop(0);
        EXPECT_EQ(arr.size(), 1ul);
        EXPECT_EQ(arr.capacity(), 3ul);
        EXPECT_FLOAT_EQ(popped, 10.0);
        EXPECT_FLOAT_EQ(arr[0], 30.0);
        EXPECT_FLOAT_EQ(arr[1], 0.0);
        EXPECT_FLOAT_EQ(arr[2], 0.0);

        popped = arr.pop(0);
        EXPECT_EQ(arr.size(), 0ul);
        EXPECT_EQ(arr.capacity(), 3ul);
        EXPECT_FLOAT_EQ(popped, 30.0);
        EXPECT_FLOAT_EQ(arr[0], 0.0);
        EXPECT_FLOAT_EQ(arr[1], 0.0);
        EXPECT_FLOAT_EQ(arr[2], 0.0);
    }

    TEST_F(TestArray, test_array_read_write)
    {
        Array<uint8_t> arr;
        uint8_t arr8[4] = {1, 2, 3, 4};
        arr.from(arr8, 4);
        EXPECT_EQ(arr.size(), 4ul);
        EXPECT_EQ(arr[0], 1);
        EXPECT_EQ(arr[1], 2);
        EXPECT_EQ(arr[2], 3);
        EXPECT_EQ(arr[3], 4);
        EXPECT_THROW(arr.at(4), std::out_of_range);
        arr[1] = 42;
        EXPECT_EQ(arr.at(1), 42);
    }

    TEST_F(TestArray, test_array_resize)
    {
        Array<uint8_t> arr;
        EXPECT_EQ(arr.size(), 0ul);
        EXPECT_EQ(arr.capacity(), 0ul);
        uint8_t arr8[4] = {1, 2, 3, 4};
        arr.from(arr8, 4);
        this->test_array(arr);
        EXPECT_EQ(arr.size(), 4ul);
        EXPECT_EQ(arr.capacity(), 4ul);
        arr.resize(8);
        EXPECT_EQ(arr.size(), 8ul);
        EXPECT_EQ(arr.capacity(), 8ul);
        EXPECT_EQ(arr.at(7), 0);
    }

    TEST_F(TestArray, test_array_reserve)
    {
        Array<uint8_t> arr;
        EXPECT_EQ(arr.size(), 0ul);
        EXPECT_EQ(arr.capacity(), 0ul);
        uint8_t arr8[4] = {1, 2, 3, 4};
        arr.from(arr8, 4);
        this->test_array(arr);
        EXPECT_EQ(arr.size(), 4ul);
        EXPECT_EQ(arr.capacity(), 4ul);
        arr.reserve(8);
        this->test_array(arr);
        EXPECT_EQ(arr.size(), 4ul);
        EXPECT_EQ(arr.capacity(), 8ul);
        EXPECT_EQ(arr[7], 0);
    }

    TEST_F(TestArray, test_array_push_back)
    {
        Array<float> arr(4);
        arr.push_back(0.1);
        arr.push_back(0.2);
        arr.push_back(0.3);
        arr.push_back(0.4);
        EXPECT_EQ(arr.size(), 4ul);
        EXPECT_EQ(arr.capacity(), 4ul);
        arr.push_back(0.5);
        EXPECT_EQ(arr.size(), 5ul);
        EXPECT_EQ(arr.capacity(), 5ul);
        EXPECT_EQ(arr[4], 0.5);
    }

    TEST_F(TestArray, test_array_push_back_array)
    {
        Array<uint8_t> arr(20);
        uint8_t arr8[4] = {1, 2, 3, 4};
        LOG(info, "Appending once");
        arr.push_back(arr8, 4);
        EXPECT_EQ(arr.size(), 4ul);
        EXPECT_EQ(arr.capacity(), 20ul);
        EXPECT_EQ(arr[0], 1);
        EXPECT_EQ(arr[1], 2);
        EXPECT_EQ(arr[2], 3);
        EXPECT_EQ(arr[3], 4);
        LOG(info, "Appending another");
        arr.push_back(arr8, 4);
        EXPECT_EQ(arr.size(), 8ul);
        EXPECT_EQ(arr.capacity(), 20ul);
        this->test_array(arr);
        LOG(info, "Appending time 2");
        arr.push_back(arr8, 4);
        arr.push_back(arr8, 4);
        EXPECT_EQ(arr.size(), 16ul);
        EXPECT_EQ(arr.capacity(), 20ul);
        LOG(info, "Appending to limit");
        arr.push_back(arr8, 4);
        EXPECT_EQ(arr.size(), 20ul);
        EXPECT_EQ(arr.capacity(), 20ul);
        this->test_array(arr);
        LOG(info, "Appending after limit");
        arr.push_back(arr8, 4);
        EXPECT_EQ(arr.size(), 24ul);
        EXPECT_EQ(arr.capacity(), 24ul);
        this->test_array(arr);
    }

    TEST_F(TestArray, test_array_assign)
    {
        uint8_t *arr8 = new uint8_t[4]{1, 2, 3, 4};
        Array<uint8_t> arr(8);
        arr.assign(arr8, 4);
        EXPECT_EQ(arr.buf(), arr8);
        EXPECT_EQ(arr.size(), 4ul);
        EXPECT_EQ(arr.capacity(), 4ul);

        arr8[0] = 10;
        arr8[1] = 20;
        arr8[2] = 30;
        arr8[3] = 40;
        EXPECT_EQ(arr[0], 10);
        EXPECT_EQ(arr[1], 20);
        EXPECT_EQ(arr[2], 30);
        EXPECT_EQ(arr[3], 40);

        arr.assign(nullptr, 0);
        EXPECT_EQ(arr.buf(), nullptr);
        EXPECT_EQ(arr.size(), 0ul);
        EXPECT_EQ(arr.capacity(), 0ul);
        delete[] arr8;
    }

    // Test storing in IBuffer
    TEST_F(TestArray, test_array_store)
    {
        sihd::util::Byte buffer_byte(200);
        buffer_byte[0] = 127;
        buffer_byte[1] = 25;
        IBuffer *buffer = &buffer_byte;
        EXPECT_EQ(buffer->buf()[0], 127);
        EXPECT_EQ(buffer->buf()[1], 25u);

        sihd::util::Int buffer_int(2);
        buffer_int[0] = 123;
        buffer_int[1] = 456;
        buffer = &buffer_int;
        EXPECT_EQ(((int *)buffer->buf())[0], 123);
        EXPECT_EQ(((int *)buffer->buf())[1], 456);
    }

    // Test all types
    TEST_F(TestArray, test_array_all)
    {
        sihd::util::Byte buffer_byte(1);
        buffer_byte[0] = 127;
        EXPECT_EQ(buffer_byte[0], 127);
        sihd::util::UByte buffer_ubyte(1);
        buffer_ubyte[0] = 255;
        EXPECT_EQ(buffer_ubyte[0], 255);

        EXPECT_EQ(buffer_byte.data_size(), sizeof(char));
        EXPECT_EQ(buffer_byte.data_type(), Datatypes::BYTE);
        EXPECT_EQ(buffer_ubyte.data_size(), sizeof(char));
        EXPECT_EQ(buffer_ubyte.data_type(), Datatypes::UBYTE);

        sihd::util::Short buffer_short(1);
        buffer_short[0] = 2;
        EXPECT_EQ(buffer_short[0], 2);
        sihd::util::UShort buffer_ushort(1);
        buffer_ushort[0] = 2;
        EXPECT_EQ(buffer_ushort[0], 2u);

        EXPECT_EQ(buffer_short.data_size(), sizeof(short));
        EXPECT_EQ(buffer_short.data_type(), Datatypes::SHORT);
        EXPECT_EQ(buffer_ushort.data_size(), sizeof(short));
        EXPECT_EQ(buffer_ushort.data_type(), Datatypes::USHORT);

        sihd::util::Int buffer_int(1);
        buffer_int[0] = 2;
        EXPECT_EQ(buffer_int[0], 2);
        sihd::util::UInt buffer_uint(1);
        buffer_uint[0] = -1;
        EXPECT_EQ(buffer_uint[0], -1u);

        EXPECT_EQ(buffer_int.data_size(), sizeof(int));
        EXPECT_EQ(buffer_int.data_type(), Datatypes::INT);
        EXPECT_EQ(buffer_uint.data_size(), sizeof(int));
        EXPECT_EQ(buffer_uint.data_type(), Datatypes::UINT);

        sihd::util::Long buffer_long(1);
        buffer_long[0] = 1;
        EXPECT_EQ(buffer_long[0], 1l);
        sihd::util::ULong buffer_ulong(1);
        buffer_ulong[0] = -1;
        EXPECT_EQ(buffer_ulong[0], -1ul);

        EXPECT_EQ(buffer_long.data_size(), sizeof(long));
        EXPECT_EQ(buffer_long.data_type(), Datatypes::LONG);
        EXPECT_EQ(buffer_ulong.data_size(), sizeof(long));
        EXPECT_EQ(buffer_ulong.data_type(), Datatypes::ULONG);

        sihd::util::Float buffer_float(1);
        buffer_float[0] = 133.7;
        EXPECT_FLOAT_EQ(buffer_float[0], 133.7f);

        EXPECT_EQ(buffer_float.data_size(), sizeof(float));
        EXPECT_EQ(buffer_float.data_type(), Datatypes::FLOAT);

        sihd::util::Double buffer_dbl(1);
        buffer_dbl[0] = 123.4;
        EXPECT_FLOAT_EQ(buffer_dbl[0], 123.4);

        EXPECT_EQ(buffer_dbl.data_size(), sizeof(double));
        EXPECT_EQ(buffer_dbl.data_type(), Datatypes::DOUBLE);
    }

    // Test from method
    TEST_F(TestArray, test_array_from)
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

}
