#include <gtest/gtest.h>

#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Runnable.hpp>

namespace test
{
using namespace sihd::util;

class TestRunnable: public ::testing::Test
{
    protected:
        TestRunnable() = default;
        virtual ~TestRunnable() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

namespace
{

struct Target
{
        bool flag = false;
        bool trigger() { return (flag = true); }
};

struct InnerRunnable: public IRunnable
{
        bool ran = false;
        bool run() override
        {
            ran = true;
            return true;
        }
};

} // namespace

TEST_F(TestRunnable, test_runnable_no_method)
{
    Runnable runnable;
    EXPECT_FALSE(runnable.has_method());
    EXPECT_FALSE(runnable.run());
    EXPECT_FALSE(runnable());
}

TEST_F(TestRunnable, test_runnable_lambda)
{
    bool called = false;
    Runnable r([&called]() -> bool {
        called = true;
        return true;
    });
    EXPECT_TRUE(r.has_method());
    EXPECT_TRUE(r.run());
    EXPECT_TRUE(called);
}

TEST_F(TestRunnable, test_runnable_set_method_member_ptr)
{
    Target target;
    Runnable runnable;
    runnable.set_method(&target, &Target::trigger);

    EXPECT_TRUE(runnable.has_method());
    EXPECT_TRUE(runnable.run());
    EXPECT_TRUE(target.flag);
}

TEST_F(TestRunnable, test_runnable_set_runnable)
{
    InnerRunnable inner;
    Runnable runnable(&inner);

    EXPECT_TRUE(runnable.has_method());
    EXPECT_TRUE(runnable.run());
    EXPECT_TRUE(inner.ran);
}

} // namespace test
