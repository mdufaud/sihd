#include <gtest/gtest.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/array_utils.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/profiling.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestArray: public ::testing::Test
{
    protected:
        TestArray() { sihd::util::LoggerManager::stream(); }

        virtual ~TestArray() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() { _array_ptr = nullptr; }

        virtual void TearDown()
        {
            if (_array_ptr != nullptr)
                delete _array_ptr;
        }

        void test_array(Array<uint8_t> & arr)
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

TEST_F(TestArray, test_array_perf)
{
    struct Test
    {
            int a;
            int b;
    };

    constexpr int test_multiplier = 1;

    Array<Test>::mult_resize_capacity = 2;
    constexpr int iterations = 30000 * test_multiplier;
    constexpr int ncopies = 100 * test_multiplier;
    std::vector<Test> vec;
    Array<Test> arr;

    Perf perf_vector("vector");
    Perf perf_array("array");
    {
        Timeit it("vector push_back");
        for (auto i = 0; i < iterations; ++i)
        {
            perf_vector.begin();
            vec.push_back({i, i + 1});
            perf_vector.end();
        }
    }
    {
        Timeit it("array push_back");
        for (auto i = 0; i < iterations; ++i)
        {
            perf_array.begin();
            arr.push_back({i, i + 1});
            perf_array.end();
        }
    }
    perf_vector.log();
    perf_array.log();
    ASSERT_TRUE(arr.is_equal(vec));
    {
        Timeit it("vector access");
        for (auto i = 0; i < iterations; ++i)
            vec[i] = vec[i];
    }
    {
        Timeit it("array access");
        for (auto i = 0; i < iterations; ++i)
            arr[i] = arr[i];
    }
    {
        Timeit it("vector iter");
        for (const auto & val : vec)
            vec[0] = val;
    }
    {
        Timeit it("array iter");
        for (const auto & val : arr)
            arr[0] = val;
    }
    {
        Timeit it("vector copy");
        for (auto i = 0; i < ncopies; ++i)
            auto vec2 = vec;
    }
    {
        Timeit it("array copy");
        for (auto i = 0; i < ncopies; ++i)
            auto arr2 = arr;
    }
}

TEST_F(TestArray, test_array_from_string)
{
    ArrInt arr_int;

    EXPECT_TRUE(arr_int.from_str("10,20,30", ","));
    EXPECT_EQ(arr_int.size(), 3u);
    EXPECT_EQ(arr_int[0], 10);
    EXPECT_EQ(arr_int[1], 20);
    EXPECT_EQ(arr_int[2], 30);
    SIHD_LOG(debug, "{}", arr_int.str(','));

    ArrInt arr2;
    EXPECT_TRUE(arr2.from_str(arr_int.str(','), ","));
    EXPECT_TRUE(arr2.is_equal(arr_int));
    SIHD_LOG(debug, "{}", arr2.str(','));

    EXPECT_FALSE(arr_int.from_str("a,b,c", ","));
    EXPECT_EQ(arr_int.size(), 0u);

    ArrFloat arr_float;
    EXPECT_TRUE(arr_float.from_str("123.456,22.1,33.2,44.3", ","));
    EXPECT_EQ(arr_float.size(), 4u);
    EXPECT_FLOAT_EQ(arr_float[0], 123.456);
    EXPECT_FLOAT_EQ(arr_float[1], 22.1);
    EXPECT_FLOAT_EQ(arr_float[2], 33.2);
    EXPECT_FLOAT_EQ(arr_float[3], 44.3);
    SIHD_LOG(debug, "{}", arr_float.str(','));

    ArrChar arr_str;
    arr_str.from_str("hello world");
    EXPECT_EQ(arr_str.str(), "hello world");
    SIHD_LOG(debug, "{}", arr_str.str());
}

TEST_F(TestArray, test_array_iterator_for)
{
    ArrInt arr = {10, 20, 30, 40};

    SIHD_LOG(debug, "Array to iter: {}", arr.str(' '));
    int val;
    int idx = 0;
    SIHD_LOG(debug, "Forward range loop");
    for (const int & i : arr)
    {
        SIHD_LOG(debug, "{}", i);
        val = i;
        ++idx;
    }
    EXPECT_EQ(val, 40);
    EXPECT_EQ(idx, 4);
    SIHD_LOG(debug, "Reverse range loop");
    for (auto it = arr.crbegin(); it != arr.crend(); ++it)
    {
        SIHD_LOG(debug, "{}", *it);
        val = *it;
        ++idx;
    }
    EXPECT_EQ(val, 10);
    EXPECT_EQ(idx, 8);
}

TEST_F(TestArray, test_array_iterator_embedded)
{
    ArrInt i;

    i.resize(4);
    std::generate(i.begin(), i.end(), [n = 1]() mutable { return n++; });
    // {1, 2, 3, 4}
    EXPECT_EQ(container::find(i, 1).idx(), 0u);
    EXPECT_EQ(container::find(i, 2).idx(), 1u);
    EXPECT_EQ(container::find(i, 4).idx(), 3u);
}

TEST_F(TestArray, test_array_iterator_algo)
{
    SIHD_LOG(debug, "Testing iterator find");

    ArrInt arr_int = {10, 20, 30, 40};

    ArrInt::iterator it_found = std::find(arr_int.begin(), arr_int.end(), 20);
    ArrInt::iterator it_not_found = std::find(arr_int.begin(), arr_int.end(), 50);

    EXPECT_NE(it_found, arr_int.end());
    EXPECT_EQ(*it_found, 20);
    EXPECT_EQ(it_not_found, arr_int.end());

    SIHD_LOG(debug, "Testing iterator algorithm");

    EXPECT_FALSE(std::binary_search(arr_int.cbegin(), arr_int.cend(), 31));
    std::for_each(arr_int.begin(), arr_int.end(), [](int & i) { ++i; });
    EXPECT_TRUE(std::binary_search(arr_int.cbegin(), arr_int.cend(), 31));

    EXPECT_EQ(std::count(arr_int.crbegin(), arr_int.crend(), 41), 1);
    EXPECT_EQ(*std::min_element(arr_int.crbegin(), arr_int.crend()), 11);
    EXPECT_EQ(*std::max_element(arr_int.crbegin(), arr_int.crend()), 41);

    ArrChar arr_str("edcba");
    SIHD_LOG(debug, "Sort before: {}", arr_str.str());
    std::sort(arr_str.begin(), arr_str.end());
    SIHD_LOG(debug, "Sort after: {}", arr_str.str());
    EXPECT_TRUE(arr_str.is_equal("abcde"));

    SIHD_LOG(debug, "Reverse before: {}", arr_str.str());
    std::reverse(arr_str.begin(), arr_str.end());
    SIHD_LOG(debug, "Reverse before: {}", arr_str.str());
    EXPECT_TRUE(arr_str.is_equal("edcba"));

    SIHD_LOG(debug, "Fill before: {}", arr_str.str());
    std::fill(arr_str.begin(), arr_str.end(), 'a');
    SIHD_LOG(debug, "Fill after: {}", arr_str.str());
    EXPECT_TRUE(arr_str.is_equal("aaaaa"));

    SIHD_LOG(debug, "Testing empty iterator");
    ArrDouble arr_dbl;
    ArrDouble::const_reverse_iterator it_dbl;
    it_dbl = arr_dbl.crbegin();
    it_dbl = std::find(arr_dbl.crbegin(), arr_dbl.crend(), 50.0);
    EXPECT_EQ(it_dbl, arr_dbl.crend());

    SIHD_LOG(debug, "Testing reverse iterator");
    const int8_t bytes[] = {1, 2, 3, 4};
    const int8_t reversed_bytes[] = {4, 3, 2, 1};
    ArrByte arr_byte(bytes, 4);

    SIHD_LOG(debug, "Reverse before: {}", arr_byte.str(' '));
    std::reverse(arr_byte.rbegin(), arr_byte.rend());
    SIHD_LOG(debug, "Reverse after: {}", arr_byte.str(' '));
    // {4, 3, 2, 1}
    EXPECT_TRUE(arr_byte.is_equal(reversed_bytes, 4));

    SIHD_LOG(debug, "Removing 4");
    auto it_remove = std::remove(arr_byte.begin(), arr_byte.end(), 4);
    // {3, 2, 1, 4}
    EXPECT_EQ(*it_remove, 1);
    auto it_rm_find = std::find(arr_byte.begin(), it_remove, 4);
    EXPECT_EQ(it_rm_find, it_remove);
    arr_byte.resize(3);
    // {3, 2, 1}
    SIHD_LOG(debug, "Remove after: {}", arr_byte.str(' '));

    SIHD_LOG(debug, "Replace 3 -> 4");
    std::replace(arr_byte.begin(), arr_byte.end(), 3, 4);
    // {4, 2, 1}
    SIHD_LOG(debug, "Replace after: {}", arr_byte.str(' '));
    EXPECT_EQ(arr_byte[0], 4);

    SIHD_LOG(debug, "Rotate left by 1");
    std::rotate(arr_byte.begin(), arr_byte.begin() + 1, arr_byte.end());
    // {2, 1, 4}
    SIHD_LOG(debug, "Rotate after: {}", arr_byte.str(' '));
    EXPECT_EQ(arr_byte[0], 2);
    EXPECT_EQ(arr_byte[1], 1);
    EXPECT_EQ(arr_byte[2], 4);
}

TEST_F(TestArray, test_array_str)
{
    const char hw[] = "hello world";
    ArrChar arr(hw);
    EXPECT_EQ(str::hexdump(arr, ','), "68,65,6c,6c,6f,20,77,6f,72,6c,64");
    EXPECT_EQ(arr.str(), "hello world");
    EXPECT_TRUE(arr.is_equal("hello world"));

    arr.push_back(" !");
    EXPECT_EQ(arr.str(), "hello world !");
    EXPECT_TRUE(arr.is_equal("hello world !"));

    char *str = strdup("test");
    EXPECT_TRUE(arr.assign(str));
    EXPECT_EQ(arr.str(), "test");
    EXPECT_TRUE(arr.is_equal("test"));
    free(str);

    EXPECT_TRUE(arr.from("derp"));
    EXPECT_EQ(arr.str(), "derp");
    EXPECT_TRUE(arr.is_equal("derp"));

    EXPECT_TRUE(arr.copy_from("aaaa"));
    EXPECT_EQ(arr.str(), "aaaa");
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

TEST_F(TestArray, test_array_push_front)
{
    ArrInt arr;

    arr.push_front(10);
    arr.push_front(5);
    arr.push_front(0);
    ASSERT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr.front(), 0);
    EXPECT_EQ(arr.back(), 10);
    EXPECT_EQ(arr[0], 0);
    EXPECT_EQ(arr[1], 5);
    EXPECT_EQ(arr[2], 10);

    arr.push_front({-10, -5});
    SIHD_LOG(debug, "{}", arr.str(' '));
    ASSERT_EQ(arr.size(), 5u);
    EXPECT_EQ(arr[0], -10);
    EXPECT_EQ(arr[1], -5);
    EXPECT_EQ(arr[2], 0);
    EXPECT_EQ(arr[3], 5);
    EXPECT_EQ(arr[4], 10);
}

TEST_F(TestArray, test_array_push_back)
{
    Array<float> arr(4);
    // capacity = 0
    arr.push_back(0.1);
    // capacity = 1
    arr.push_back(0.2);
    // capacity = 1 * 2 = 2
    arr.push_back(0.3);
    // capacity = 2 * 2 = 4
    arr.push_back(0.4);
    // capacity = 4
    EXPECT_EQ(arr.size(), 4ul);
    EXPECT_EQ(arr.capacity(), 4ul);
    arr.push_back({0.5, 0.6});
    // capacity = 4 * 2 = 8
    ASSERT_EQ(arr.size(), 6ul);
    EXPECT_EQ(arr.capacity(), 8ul);
    EXPECT_FLOAT_EQ(arr[4], 0.5);
    EXPECT_FLOAT_EQ(arr[5], 0.6);
}

TEST_F(TestArray, test_array_push_back_array)
{
    Array<uint8_t> arr(20);
    uint8_t arr8[4] = {1, 2, 3, 4};
    SIHD_LOG(info, "Appending once");
    arr.push_back(arr8, 4);
    EXPECT_EQ(arr.size(), 4ul);
    EXPECT_EQ(arr.capacity(), 20ul);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 4);
    SIHD_LOG(info, "Appending another");
    arr.push_back(arr8, 4);
    EXPECT_EQ(arr.size(), 8ul);
    EXPECT_EQ(arr.capacity(), 20ul);
    this->test_array(arr);
    SIHD_LOG(info, "Appending time 2");
    arr.push_back(arr8, 4);
    arr.push_back(arr8, 4);
    EXPECT_EQ(arr.size(), 16ul);
    EXPECT_EQ(arr.capacity(), 20ul);
    SIHD_LOG(info, "Appending to limit");
    arr.push_back(arr8, 4);
    EXPECT_EQ(arr.size(), 20ul);
    EXPECT_EQ(arr.capacity(), 20ul);
    this->test_array(arr);
    SIHD_LOG(info, "Appending after limit");
    arr.push_back(arr8, 4);
    EXPECT_EQ(arr.size(), 24ul);
    // capacity = 20 * 2 = 40
    EXPECT_EQ(arr.capacity(), 40ul);
    this->test_array(arr);
}

TEST_F(TestArray, test_array_assign)
{
    uint8_t *arr8 = new uint8_t[4] {1, 2, 3, 4};
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
    EXPECT_EQ(buffer_byte.data_type(), Type::TYPE_BYTE);
    EXPECT_EQ(buffer_ubyte.data_size(), sizeof(char));
    EXPECT_EQ(buffer_ubyte.data_type(), Type::TYPE_UBYTE);

    sihd::util::ArrShort buffer_short(1);
    buffer_short[0] = 2;
    EXPECT_EQ(buffer_short[0], 2);
    sihd::util::ArrUShort buffer_ushort(1);
    buffer_ushort[0] = 2;
    EXPECT_EQ(buffer_ushort[0], 2u);

    EXPECT_EQ(buffer_short.data_size(), sizeof(short));
    EXPECT_EQ(buffer_short.data_type(), Type::TYPE_SHORT);
    EXPECT_EQ(buffer_ushort.data_size(), sizeof(short));
    EXPECT_EQ(buffer_ushort.data_type(), Type::TYPE_USHORT);

    sihd::util::ArrInt buffer_int(1);
    buffer_int[0] = 2;
    EXPECT_EQ(buffer_int[0], 2);
    sihd::util::ArrUInt buffer_uint(1);
    buffer_uint[0] = -1;
    EXPECT_EQ(buffer_uint[0], -1u);

    EXPECT_EQ(buffer_int.data_size(), sizeof(int));
    EXPECT_EQ(buffer_int.data_type(), Type::TYPE_INT);
    EXPECT_EQ(buffer_uint.data_size(), sizeof(int));
    EXPECT_EQ(buffer_uint.data_type(), Type::TYPE_UINT);

    sihd::util::ArrLong buffer_long(1);
    buffer_long[0] = 1;
    EXPECT_EQ(buffer_long[0], 1l);
    sihd::util::ArrULong buffer_ulong(1);
    buffer_ulong[0] = -1;
    EXPECT_EQ(buffer_ulong[0], -1ul);

    EXPECT_EQ(buffer_long.data_size(), sizeof(long));
    EXPECT_EQ(buffer_long.data_type(), Type::TYPE_LONG);
    EXPECT_EQ(buffer_ulong.data_size(), sizeof(long));
    EXPECT_EQ(buffer_ulong.data_type(), Type::TYPE_ULONG);

    sihd::util::ArrFloat buffer_float(1);
    buffer_float[0] = 133.7;
    EXPECT_FLOAT_EQ(buffer_float[0], 133.7f);

    EXPECT_EQ(buffer_float.data_size(), sizeof(float));
    EXPECT_EQ(buffer_float.data_type(), Type::TYPE_FLOAT);

    sihd::util::ArrDouble buffer_dbl(1);
    buffer_dbl[0] = 123.4;
    EXPECT_FLOAT_EQ(buffer_dbl[0], 123.4);

    EXPECT_EQ(buffer_dbl.data_size(), sizeof(double));
    EXPECT_EQ(buffer_dbl.data_type(), Type::TYPE_DOUBLE);
}

TEST_F(TestArray, test_array_move)
{
    ArrInt arr;
    ArrInt arr_moved;
    int *buffer_ptr;

    arr.from({1, 2, 3});
    buffer_ptr = arr.data();
    arr_moved = std::move(arr);
    EXPECT_EQ(arr.data(), nullptr);
    EXPECT_EQ(arr_moved.data(), buffer_ptr);
    EXPECT_EQ(arr_moved.size(), 3u);

    ArrInt arr_move_constructor(std::move(arr_moved));
    EXPECT_EQ(arr_moved.data(), nullptr);
    EXPECT_EQ(arr_move_constructor.data(), buffer_ptr);
    EXPECT_EQ(arr_move_constructor.size(), 3u);
}

TEST_F(TestArray, test_array_equal)
{
    ArrayUnique<int> arr_int = std::make_unique<Array<int>>(Array<int> {1, 2, 3});
    int tbl_all[3] = {1, 2, 3};
    int tbl_two[2] = {2, 3};

    EXPECT_TRUE(arr_int->is_equal(*arr_int));
    EXPECT_TRUE(arr_int->is_equal({1, 2, 3}));
    EXPECT_TRUE(arr_int->is_equal({2, 3}, 1));
    EXPECT_TRUE(arr_int->is_bytes_equal((const uint8_t *)tbl_all, 3 * sizeof(int)));
    EXPECT_TRUE(arr_int->is_bytes_equal((const uint8_t *)tbl_two, 2 * sizeof(int), 1 * sizeof(int)));
}

TEST_F(TestArray, test_array_from)
{
    ArrInt buffer(2);

    buffer.resize(2);
    buffer[0] = 13;
    buffer[1] = 37;
    EXPECT_EQ(buffer[0], 13);
    EXPECT_EQ(buffer[1], 37);

    ArrInt buffer_copied;
    EXPECT_EQ(buffer_copied.from(buffer), true);
    EXPECT_EQ(buffer_copied[0], 13);
    EXPECT_EQ(buffer_copied[1], 37);

    ArrByte buffer_byte;
    buffer_byte.resize(3);
    ArrFloat buffer_impossible_copy;
    EXPECT_THROW(buffer_impossible_copy.from_bytes(buffer_byte), std::invalid_argument);

    ArrInt *buffer_clone = buffer.clone();
    EXPECT_EQ(buffer_clone->at(0), 13);
    EXPECT_EQ(buffer_clone->at(1), 37);
    delete buffer_clone;
}

TEST_F(TestArray, test_array_util_create)
{
    _array_ptr = array_utils::create_from_type(TYPE_INT, 1);
    EXPECT_NE(_array_ptr, nullptr);
    _array_ptr->resize(1);
    EXPECT_EQ(array_utils::read<int>(_array_ptr, 0), 0);
    EXPECT_TRUE(array_utils::write<int>(_array_ptr, 0, 20));
    EXPECT_EQ(array_utils::read<int>(_array_ptr, 0), 20);
}

TEST_F(TestArray, test_array_std)
{
    std::array<int, 3> std_arr({1, 2, 3});
    std::vector<int> std_vec({4, 5, 6});

    Array<int> arr(std_arr);
    EXPECT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr.at(0), 1);
    EXPECT_EQ(arr.at(1), 2);
    EXPECT_EQ(arr.at(2), 3);

    Array<int> arr2(std_vec);
    EXPECT_EQ(arr2.size(), 3u);
    EXPECT_EQ(arr2.at(0), 4);
    EXPECT_EQ(arr2.at(1), 5);
    EXPECT_EQ(arr2.at(2), 6);
}

TEST_F(TestArray, test_array_struct)
{
    struct Test
    {
            int x;
            int y;
    };

    Array<Test> arr;
    arr.push_back({
        .x = 42,
        .y = 1337,
    });
    EXPECT_EQ(arr[0].x, 42);
    EXPECT_EQ(arr[0].y, 1337);
    EXPECT_EQ(arr.size(), 1UL);

    SIHD_LOG_DEBUG("{}", arr.str(' '));

    auto arr2 = arr;
    EXPECT_TRUE(arr.is_equal(arr2));

    EXPECT_EQ(arr.str(' '), "2a 00 00 00 39 05 00 00");
    EXPECT_TRUE(arr.from_str("2a 00 00 00 39 05 00 00 2a 00 00 00 39 05 00 00", " "));

    EXPECT_EQ(arr[0].x, 42);
    EXPECT_EQ(arr[0].y, 1337);
    EXPECT_EQ(arr[1].x, 42);
    EXPECT_EQ(arr[1].y, 1337);
    EXPECT_EQ(arr.size(), 2UL);

    EXPECT_EQ(arr.str(' '), "2a 00 00 00 39 05 00 00 2a 00 00 00 39 05 00 00");
    EXPECT_FALSE(arr.from_str("2a", " "));
}

} // namespace test
