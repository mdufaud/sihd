#include <gtest/gtest.h>

#include <sihd/util/ScopedModifier.hpp>

namespace test
{
using namespace sihd::util;

class TestScopedModifier: public ::testing::Test
{
    protected:
        TestScopedModifier() = default;
        virtual ~TestScopedModifier() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestScopedModifier, test_scoped_modifier_restores)
{
    int val = 10;
    {
        ScopedModifier m(val, 42);
        EXPECT_EQ(val, 42);
    }
    EXPECT_EQ(val, 10);
}

TEST_F(TestScopedModifier, test_scoped_modifier_bool)
{
    bool flag = false;
    {
        ScopedModifier m(flag, true);
        EXPECT_TRUE(flag);
    }
    EXPECT_FALSE(flag);
}

} // namespace test
