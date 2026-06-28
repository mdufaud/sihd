#include <gtest/gtest.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/container.hpp>

namespace test
{
SIHD_LOGGER;

using namespace sihd::util;
class TestArrayView: public ::testing::Test
{
    protected:
        TestArrayView() { sihd::util::LoggerManager::stream(); }

        virtual ~TestArrayView() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        template <typename T>
        size_t get_size(ArrayView<T> view)
        {
            return view.size();
        }

        size_t get_byte_size(ArrByteView view) { return view.size(); }

        size_t get_str_size(ArrCharView view) { return view.size(); }

        size_t print_array(ArrIntView view)
        {
            for (int i : view)
            {
                SIHD_LOG(debug, "{}", i);
            }
            return view.size();
        }
};

TEST_F(TestArrayView, test_arrayview_fundamental)
{
    int val = 5;
    ArrByteView view_int(val);
    ASSERT_EQ(view_int.size(), sizeof(int));

    int8_t *byte = (int8_t *)&val;
    EXPECT_EQ(view_int[0], byte[0]);
    EXPECT_EQ(view_int[1], byte[1]);
    EXPECT_EQ(view_int[2], byte[2]);
    EXPECT_EQ(view_int[3], byte[3]);

    ArrByteView view_double(5.0);
    // CANNOT ACCESS BECAUSE THE POINTER OF THE DOUBLE IS NOW LOST - BEWARE
    ASSERT_EQ(view_double.size(), sizeof(double));
}

TEST_F(TestArrayView, test_arrayview_struct)
{
    struct Test
    {
            int x;
            int y;
    };

    Test mono;
    mono.x = 42;
    mono.y = 1337;

    ArrayView<Test> mono_view(mono);
    EXPECT_EQ(mono_view[0].x, 42);
    EXPECT_EQ(mono_view[0].y, 1337);

    ArrByteView byte_mono_view(mono);
    EXPECT_EQ(byte_mono_view.size(), sizeof(Test));

    Test arr[3];
    arr[0].x = 42;
    arr[0].y = 1337;

    ArrayView<Test> view(arr, 3);
    EXPECT_EQ(view[0].x, 42);
    EXPECT_EQ(view[0].y, 1337);
}

TEST_F(TestArrayView, test_arrayview_init)
{
    // initializer list can work only when stack is scoped
    EXPECT_EQ(TestArrayView::print_array({1, 2, 3}), 3u);
}

TEST_F(TestArrayView, test_arrayview_str)
{
    ArrCharView view_str("hello world");

    EXPECT_TRUE(view_str.is_equal("hello world"));
    EXPECT_EQ(view_str.str(), "hello world");

    EXPECT_TRUE(view_str.subview({0, 4}).is_equal("hello"));
    EXPECT_TRUE(view_str.subview({6, -1}).is_equal("world"));
    EXPECT_TRUE(view_str.subview({10000, -1}).empty());
    EXPECT_TRUE(view_str.subview({10000, 20000}).empty());

    view_str.remove_prefix(6);
    EXPECT_TRUE(view_str.is_equal("world"));
    view_str.remove_suffix(4);
    EXPECT_TRUE(view_str.is_equal("w"));
    view_str.remove_suffix(4);
    EXPECT_TRUE(view_str.is_equal(""));

    std::string str = "toto";
    ArrCharView view_str2(str);
    EXPECT_EQ(view_str2.size(), str.size());
    EXPECT_TRUE(view_str2.is_equal(str.data()));
    EXPECT_TRUE(view_str2.is_equal(view_str2));
    EXPECT_TRUE(view_str2.is_equal(str));

    view_str2.remove_prefix(-1);
    EXPECT_EQ(view_str2.size(), 0u);
    EXPECT_TRUE(view_str2.is_equal(""));

    ArrChar arr_char(str);
    ArrCharView arr_view_char(arr_char);
    EXPECT_TRUE(arr_char.is_equal(str));
    EXPECT_TRUE(arr_char.is_bytes_equal(arr_view_char));
    EXPECT_TRUE(arr_view_char.is_equal(arr_char));
    EXPECT_TRUE(arr_view_char.is_equal(str));

    EXPECT_EQ(TestArrayView::get_str_size("hello"), 5u);
}

TEST_F(TestArrayView, test_arrayview_buf)
{
    int buf[3] = {1, 2, 3};
    ArrayView<int> view_int(buf, 3);

    ASSERT_EQ(view_int.size(), 3u);
    EXPECT_EQ(view_int[0], 1);
    EXPECT_EQ(view_int[1], 2);
    EXPECT_EQ(view_int[2], 3);
    EXPECT_TRUE(view_int.is_equal(buf, 3));
    EXPECT_EQ(TestArrayView::get_size<int>({buf, 3}), 3u);
    EXPECT_EQ(TestArrayView::get_byte_size({buf, 3 * sizeof(int)}), 3 * sizeof(int));
    view_int.remove_prefix(-1);
    ASSERT_EQ(view_int.size(), 0u);
}

TEST_F(TestArrayView, test_arrayview_array)
{
    Array<int> arr_int({1, 2, 3, 4});
    ArrayView<int> view_int(arr_int);

    EXPECT_EQ(view_int.data(), arr_int.data());
    ASSERT_EQ(view_int.size(), 4u);
    EXPECT_EQ(view_int.byte_size(), sizeof(int) * 4);
    EXPECT_TRUE(view_int.is_equal(arr_int));
    EXPECT_TRUE(arr_int.is_bytes_equal(view_int));
    EXPECT_EQ(view_int[0], 1);
    EXPECT_EQ(view_int[1], 2);
    EXPECT_EQ(view_int[2], 3);
    EXPECT_EQ(view_int[3], 4);
    EXPECT_EQ(TestArrayView::get_size<int>(arr_int), arr_int.size());
}

TEST_F(TestArrayView, test_arrayview_vector)
{
    std::vector<int> vec = {1, 2, 3, 4};
    ArrayView<int> view_int(vec);

    EXPECT_EQ(view_int.data(), vec.data());
    ASSERT_EQ(view_int.size(), 4u);
    EXPECT_EQ(view_int.byte_size(), sizeof(int) * 4);
    EXPECT_TRUE(view_int.is_equal(vec));
    EXPECT_EQ(view_int[0], 1);
    EXPECT_EQ(view_int[1], 2);
    EXPECT_EQ(view_int[2], 3);
    EXPECT_EQ(view_int[3], 4);
    EXPECT_EQ(container::find(view_int, 1).idx(), 0u);
    EXPECT_EQ(container::find(view_int, 2).idx(), 1u);
    EXPECT_EQ(container::find(view_int, 3).idx(), 2u);
    EXPECT_EQ(TestArrayView::get_size<int>(vec), vec.size());
}

TEST_F(TestArrayView, test_arrayview_to_span)
{
    Array<int> arr_int({1, 2, 3, 4});
    ArrayView<int> view_int(arr_int);
    std::span<const int> span(view_int);

    EXPECT_EQ(span.size(), arr_int.size());
    EXPECT_EQ(span.data(), arr_int.data());
    ASSERT_EQ(span.size_bytes(), arr_int.byte_size());
    EXPECT_TRUE(view_int.is_equal(span));
}

TEST(TestArrayViewSpan, test_arrayview_from_span)
{
    int arr[] = {10, 20, 30, 40};
    std::span<int> span(arr, 4);

    ArrayView<int> view(span);

    ASSERT_EQ(view.size(), 4u);
    EXPECT_EQ(view[0], 10);
    EXPECT_EQ(view[1], 20);
    EXPECT_EQ(view[2], 30);
    EXPECT_EQ(view[3], 40);

    // Check that data pointers match
    EXPECT_EQ(view.data(), span.data());

    // Check is_equal with span
    EXPECT_TRUE(view.is_equal(span));
}

TEST(TestArrayViewSpan, test_arrayview_from_span_char)
{
    std::string str = "test";
    std::span<const char> span(str);

    ArrCharView view(span);

    ASSERT_EQ(view.size(), str.size());
    ASSERT_EQ(view.size(), span.size());
    ASSERT_EQ(view.byte_size(), span.size_bytes());
    EXPECT_TRUE(view.is_equal(str));
    EXPECT_EQ(view.str(), str);
}

TEST(TestArrayViewSlice, test_arrayview_subview_slice)
{
    Array<int> arr({10, 20, 30, 40, 50});
    ArrayView<int> view(arr);

    auto sub = view.subview({1, 3});
    ASSERT_EQ(sub.size(), 3u);
    EXPECT_EQ(sub[0], 20);
    EXPECT_EQ(sub[1], 30);
    EXPECT_EQ(sub[2], 40);
}

TEST(TestArrayViewSlice, test_arrayview_subview_slice_negative)
{
    Array<int> arr({10, 20, 30, 40, 50});
    ArrayView<int> view(arr);

    auto sub = view.subview({-2, -1});
    ASSERT_EQ(sub.size(), 2u);
    EXPECT_EQ(sub[0], 40);
    EXPECT_EQ(sub[1], 50);
}

TEST(TestArrayViewSlice, test_arrayview_copy_to_bytes_slice)
{
    Array<uint8_t> arr({0x11, 0x22, 0x33, 0x44, 0x55});
    ArrayView<uint8_t> view(arr);
    uint8_t dst[3] = {};

    EXPECT_TRUE(view.copy_to_bytes(dst, {1, 3}));
    EXPECT_EQ(dst[0], 0x22);
    EXPECT_EQ(dst[1], 0x33);
    EXPECT_EQ(dst[2], 0x44);
}

TEST(TestArrayViewSlice, test_arrayview_subview_slice_empty)
{
    Array<int> arr({10, 20, 30, 40, 50});
    ArrayView<int> view(arr);

    EXPECT_TRUE(view.subview({10, 3}).empty());
    EXPECT_TRUE(view.subview({3, 1}).empty());
    EXPECT_TRUE(view.subview({-1, -3}).empty());

    uint8_t buf[4] = {};
    EXPECT_FALSE(view.copy_to_bytes(buf, {100, 50}));
}

TEST(TestArrayViewSlice, test_arrayview_subview_slice_clamp)
{
    Array<int> arr({10, 20, 30});
    ArrayView<int> view(arr);

    auto sub = view.subview({0, 100});
    ASSERT_EQ(sub.size(), 3u);
    EXPECT_EQ(sub[0], 10);
    EXPECT_EQ(sub[1], 20);
    EXPECT_EQ(sub[2], 30);

    auto sub2 = view.subview({-100, -1});
    ASSERT_EQ(sub2.size(), 3u);
    EXPECT_EQ(sub2[0], 10);
}

} // namespace test
