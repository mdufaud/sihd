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
                _array_ptr = nullptr;
            }

            virtual void TearDown()
            {
                if (_array_ptr != nullptr)
                    delete _array_ptr;
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

            IArray *_array_ptr;
    };

    TEST_F(TestArray, test_array_iterator_for)
    {
        ArrInt arr = {10, 20, 30, 40};

        int val;
        int idx = 0;
        for (const int & i: arr)
        {
            val = i;
            ++idx;
        }
        EXPECT_EQ(val, 40);
        EXPECT_EQ(idx, 4);
        for (auto it = arr.crbegin(); it != arr.crend(); ++it)
        {
            val = *it;
            ++idx;
        }
        EXPECT_EQ(val, 10);
        EXPECT_EQ(idx, 8);
    }

    TEST_F(TestArray, test_array_iterator_algo)
    {
        ArrInt arr_int = {10, 20, 30, 40};

        ArrInt::iterator it_found = std::find(arr_int.begin(), arr_int.end(), 20);
        ArrInt::iterator it_not_found = std::find(arr_int.begin(), arr_int.end(), 50);

        EXPECT_NE(it_found, arr_int.end());
        EXPECT_EQ(*it_found, 20);
        EXPECT_EQ(it_not_found, arr_int.end());

        ArrStr arr_char("edcba");

        LOG(debug, "Sort before: " << arr_char.to_string());
        std::sort(arr_char.begin(), arr_char.end());
        LOG(debug, "Sort after: " << arr_char.to_string());
        EXPECT_TRUE(arr_char.is_equal("abcde"));

        LOG(debug, "Reverse before: " << arr_char.to_string());
        std::reverse(arr_char.begin(), arr_char.end());
        LOG(debug, "Reverse before: " << arr_char.to_string());
        EXPECT_TRUE(arr_char.is_equal("edcba"));

        LOG(debug, "Fill before: " << arr_char.to_string());
        std::fill(arr_char.begin(), arr_char.end(), 'a');
        LOG(debug, "Fill after: " << arr_char.to_string());

        size_t i = 0;
        while (i < arr_char.size())
        {
            EXPECT_EQ(arr_char[i], 'a');
            ++i;
        }

        // empty iterator
        ArrDouble arr_dbl;
        ArrDouble::const_reverse_iterator it_dbl;
        it_dbl = arr_dbl.crbegin();
        it_dbl = std::find(arr_dbl.crbegin(), arr_dbl.crend(), 50.0);
        EXPECT_EQ(it_dbl, arr_dbl.crend());

        // reverse iterator
        const int8_t bytes[] = {1, 2, 3, 4};
        const int8_t reversed_bytes[] = {4, 3, 2, 1};
        ArrByte arr_byte(bytes, 4);

        TRACE(arr_byte.hexdump());
        std::reverse(arr_byte.rbegin(), arr_byte.rend());
        TRACE(arr_byte.to_string());
        EXPECT_TRUE(arr_byte.is_equal(reversed_bytes, 4));
    }

    TEST_F(TestArray, test_array_str)
    {
        const char hw[] = "hello world";
        ArrStr arr(hw);
        EXPECT_EQ(arr.hexdump(','), "68,65,6c,6c,6f,20,77,6f,72,6c,64");
        EXPECT_EQ(arr.to_string(), "hello world");
        EXPECT_TRUE(arr.is_equal("hello world"));

        arr.push_back(" !");
        EXPECT_EQ(arr.to_string(), "hello world !");
        EXPECT_TRUE(arr.is_equal("hello world !"));

        char *str = strdup("test");
        EXPECT_TRUE(arr.assign(str));
        EXPECT_EQ(arr.to_string(), "test");
        EXPECT_TRUE(arr.is_equal("test"));
        free(str);

        EXPECT_TRUE(arr.from("derp"));
        EXPECT_EQ(arr.to_string(), "derp");
        EXPECT_TRUE(arr.is_equal("derp"));

        EXPECT_TRUE(arr.copy_from("aaaa"));
        EXPECT_EQ(arr.to_string(), "aaaa");
        EXPECT_TRUE(arr.is_equal("aaaa"));
    }

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

        EXPECT_EQ(arr[0], 1);
        EXPECT_EQ(arr[1], 2);
        EXPECT_EQ(arr[2], 3);
        EXPECT_EQ(arr[3], 4);
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

    // Test storing in IArray
    TEST_F(TestArray, test_array_store)
    {
        sihd::util::ArrByte buffer_byte(200);
        buffer_byte[0] = 127;
        buffer_byte[1] = 25;
        IArray *buffer = &buffer_byte;
        EXPECT_EQ(buffer->buf()[0], 127);
        EXPECT_EQ(buffer->buf()[1], 25u);

        sihd::util::ArrInt buffer_int(2);
        buffer_int[0] = 123;
        buffer_int[1] = 456;
        buffer = &buffer_int;
        EXPECT_EQ(((int *)buffer->buf())[0], 123);
        EXPECT_EQ(((int *)buffer->buf())[1], 456);
    }

    // Test all types
    TEST_F(TestArray, test_array_all)
    {
        sihd::util::ArrByte buffer_byte(1);
        buffer_byte[0] = 127;
        EXPECT_EQ(buffer_byte[0], 127);
        sihd::util::ArrUByte buffer_ubyte(1);
        buffer_ubyte[0] = 255;
        EXPECT_EQ(buffer_ubyte[0], 255);

        EXPECT_EQ(buffer_byte.data_size(), sizeof(char));
        EXPECT_EQ(buffer_byte.data_type(), Type::DBYTE);
        EXPECT_EQ(buffer_ubyte.data_size(), sizeof(char));
        EXPECT_EQ(buffer_ubyte.data_type(), Type::DUBYTE);

        sihd::util::ArrShort buffer_short(1);
        buffer_short[0] = 2;
        EXPECT_EQ(buffer_short[0], 2);
        sihd::util::ArrUShort buffer_ushort(1);
        buffer_ushort[0] = 2;
        EXPECT_EQ(buffer_ushort[0], 2u);

        EXPECT_EQ(buffer_short.data_size(), sizeof(short));
        EXPECT_EQ(buffer_short.data_type(), Type::DSHORT);
        EXPECT_EQ(buffer_ushort.data_size(), sizeof(short));
        EXPECT_EQ(buffer_ushort.data_type(), Type::DUSHORT);

        sihd::util::ArrInt buffer_int(1);
        buffer_int[0] = 2;
        EXPECT_EQ(buffer_int[0], 2);
        sihd::util::ArrUInt buffer_uint(1);
        buffer_uint[0] = -1;
        EXPECT_EQ(buffer_uint[0], -1u);

        EXPECT_EQ(buffer_int.data_size(), sizeof(int));
        EXPECT_EQ(buffer_int.data_type(), Type::DINT);
        EXPECT_EQ(buffer_uint.data_size(), sizeof(int));
        EXPECT_EQ(buffer_uint.data_type(), Type::DUINT);

        sihd::util::ArrLong buffer_long(1);
        buffer_long[0] = 1;
        EXPECT_EQ(buffer_long[0], 1l);
        sihd::util::ArrULong buffer_ulong(1);
        buffer_ulong[0] = -1;
        EXPECT_EQ(buffer_ulong[0], -1ul);

        EXPECT_EQ(buffer_long.data_size(), sizeof(long));
        EXPECT_EQ(buffer_long.data_type(), Type::DLONG);
        EXPECT_EQ(buffer_ulong.data_size(), sizeof(long));
        EXPECT_EQ(buffer_ulong.data_type(), Type::DULONG);

        sihd::util::ArrFloat buffer_float(1);
        buffer_float[0] = 133.7;
        EXPECT_FLOAT_EQ(buffer_float[0], 133.7f);

        EXPECT_EQ(buffer_float.data_size(), sizeof(float));
        EXPECT_EQ(buffer_float.data_type(), Type::DFLOAT);

        sihd::util::ArrDouble buffer_dbl(1);
        buffer_dbl[0] = 123.4;
        EXPECT_FLOAT_EQ(buffer_dbl[0], 123.4);

        EXPECT_EQ(buffer_dbl.data_size(), sizeof(double));
        EXPECT_EQ(buffer_dbl.data_type(), Type::DDOUBLE);
    }

    // Test from method
    TEST_F(TestArray, test_array_from)
    {
        sihd::util::ArrInt buffer(2);

        buffer[0] = 13;
        buffer[1] = 37;
        EXPECT_EQ(buffer[0], 13);
        EXPECT_EQ(buffer[1], 37);

        sihd::util::ArrInt buffer_copied;
        EXPECT_EQ(buffer_copied.from(buffer), true);
        EXPECT_EQ(buffer_copied[0], 13);
        EXPECT_EQ(buffer_copied[1], 37);

        sihd::util::ArrFloat buffer_impossible_copy;
        EXPECT_EQ(buffer_impossible_copy.from(buffer), false);

        ArrInt *buffer_clone = buffer.clone();
        EXPECT_EQ(buffer_clone->at(0), 13);
        EXPECT_EQ(buffer_clone->at(1), 37);
        delete buffer_clone;
    }

    TEST_F(TestArray, test_array_util_create)
    {
        _array_ptr = ArrayUtil::create_from_type(DINT, 1);
        EXPECT_NE(_array_ptr, nullptr);
        _array_ptr->resize(1);
        EXPECT_EQ(ArrayUtil::read_array<int>(_array_ptr, 0), 0);
        EXPECT_TRUE(ArrayUtil::write_array<int>(_array_ptr, 0, 20));
        EXPECT_EQ(ArrayUtil::read_array<int>(_array_ptr, 0), 20);
    }
}
