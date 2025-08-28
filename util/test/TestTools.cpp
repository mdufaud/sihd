#include <gtest/gtest.h>

#include <sihd/util/tools.hpp>

namespace test
{
using namespace sihd::util;
class TestTools: public ::testing::Test
{
    protected:
        TestTools() = default;

        virtual ~TestTools() = default;

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestTools, test_tools_boolean)
{
    EXPECT_TRUE(tools::only_one_true(true));
    EXPECT_TRUE(tools::maximum_one_true(true));
    EXPECT_TRUE(tools::maximum_one_true(false));

    EXPECT_TRUE(tools::only_one_true(true, false, false));
    EXPECT_TRUE(tools::only_one_true(false, true, false));
    EXPECT_TRUE(tools::only_one_true(false, false, true));

    EXPECT_TRUE(tools::maximum_one_true(false, false, true));
    EXPECT_TRUE(tools::maximum_one_true(false, false, false));

    EXPECT_FALSE(tools::only_one_true(false));

    EXPECT_FALSE(tools::only_one_true(true, true, false));
    EXPECT_FALSE(tools::only_one_true(true, false, true));
    EXPECT_FALSE(tools::only_one_true(true, false, true));
    EXPECT_FALSE(tools::only_one_true(false, false, false));
    EXPECT_FALSE(tools::only_one_true(true, true, true));

    EXPECT_FALSE(tools::maximum_one_true(true, true, false));
    EXPECT_FALSE(tools::maximum_one_true(true, true, true));

    int i = 1;
    EXPECT_TRUE(tools::only_one_true(i == 1, i != 1, i < 0));

    // Testing false

    EXPECT_TRUE(tools::only_one_false(false));
    EXPECT_TRUE(tools::maximum_one_false(true));
    EXPECT_TRUE(tools::maximum_one_false(false));

    EXPECT_TRUE(tools::maximum_one_false(true, true, true));
    EXPECT_TRUE(tools::maximum_one_false(false, true, true));

    EXPECT_TRUE(tools::only_one_false(false, true, true));
    EXPECT_TRUE(tools::only_one_false(true, false, true));
    EXPECT_TRUE(tools::only_one_false(true, true, false));

    EXPECT_FALSE(tools::only_one_false(true));

    EXPECT_FALSE(tools::only_one_false(true, false, false));
    EXPECT_FALSE(tools::only_one_false(false, true, false));
    EXPECT_FALSE(tools::only_one_false(false, false, true));
    EXPECT_FALSE(tools::only_one_false(false, false, false));
    EXPECT_FALSE(tools::only_one_false(true, true, true));

    EXPECT_FALSE(tools::maximum_one_false(false, false, true));
    EXPECT_FALSE(tools::maximum_one_false(false, false, false));
}

} // namespace test
