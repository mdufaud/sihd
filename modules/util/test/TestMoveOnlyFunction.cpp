#include <gtest/gtest.h>

#include <sihd/util/MoveOnlyFunction.hpp>

namespace test
{
using namespace sihd::util;

class TestMoveOnlyFunction: public ::testing::Test
{
    protected:
        TestMoveOnlyFunction() = default;
        virtual ~TestMoveOnlyFunction() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

namespace
{
struct MoveOnly
{
        int value = 0;

        explicit MoveOnly(int value): value(value) {}
        MoveOnly(MoveOnly &&) = default;
        MoveOnly(const MoveOnly &) = delete;
};
} // namespace

TEST_F(TestMoveOnlyFunction, test_empty)
{
    MoveOnlyFunction<int(int)> fn;
    EXPECT_FALSE(static_cast<bool>(fn));

    fn = nullptr;
    EXPECT_FALSE(static_cast<bool>(fn));
}

TEST_F(TestMoveOnlyFunction, test_call)
{
    MoveOnlyFunction<int(int)> fn = [](int value) {
        return value * 2;
    };
    EXPECT_TRUE(static_cast<bool>(fn));
    EXPECT_EQ(fn(21), 42);

    MoveOnlyFunction<void(int &)> increment = [](int & value) {
        ++value;
    };
    int value = 1;
    increment(value);
    EXPECT_EQ(value, 2);
}

TEST_F(TestMoveOnlyFunction, test_move_only_callable)
{
    auto make = []() {
        return MoveOnlyFunction<int()>([file = MoveOnly(21)] { return file.value * 2; });
    };
    MoveOnlyFunction<int()> fn = make();
    EXPECT_EQ(fn(), 42);

    MoveOnlyFunction<int()> moved = std::move(fn);
    EXPECT_FALSE(static_cast<bool>(fn));
    EXPECT_EQ(moved(), 42);
}

TEST_F(TestMoveOnlyFunction, test_move_assign_and_reset)
{
    MoveOnlyFunction<std::string(const char *)> fn = [](const char *str) {
        return std::string(str);
    };
    MoveOnlyFunction<std::string(const char *)> moved = std::move(fn);
    EXPECT_EQ(moved("abc"), "abc");

    moved = nullptr;
    EXPECT_FALSE(static_cast<bool>(moved));
}

} // namespace test
