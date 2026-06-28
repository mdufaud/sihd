#include <gtest/gtest.h>

#include <sihd/util/Defer.hpp>

namespace test
{
using namespace sihd::util;

class TestDefer: public ::testing::Test
{
    protected:
        TestDefer() = default;
        virtual ~TestDefer() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestDefer, test_defer_calls_on_scope_exit)
{
    bool called = false;
    {
        Defer d([&called] { called = true; });
        EXPECT_FALSE(called);
    }
    EXPECT_TRUE(called);
}

TEST_F(TestDefer, test_defer_order)
{
    std::string order;
    {
        Defer d1([&order] { order += "1"; });
        Defer d2([&order] { order += "2"; });
    }
    EXPECT_EQ(order, "21");
}

} // namespace test
