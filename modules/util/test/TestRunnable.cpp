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

TEST_F(TestRunnable, test_runnable_no_method)
{
    Runnable r;
    EXPECT_FALSE(r.has_method());
    EXPECT_FALSE(r.run());
    EXPECT_FALSE(r());
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
    struct Target
    {
            bool flag = false;
            bool trigger() { return (flag = true); }
    };

    Target t;
    Runnable r;
    r.set_method(&t, &Target::trigger);

    EXPECT_TRUE(r.has_method());
    EXPECT_TRUE(r.run());
    EXPECT_TRUE(t.flag);
}

TEST_F(TestRunnable, test_runnable_set_runnable)
{
    struct InnerRunnable: public IRunnable
    {
            bool ran = false;
            bool run() override
            {
                ran = true;
                return true;
            }
    };

    InnerRunnable inner;
    Runnable r(&inner);

    EXPECT_TRUE(r.has_method());
    EXPECT_TRUE(r.run());
    EXPECT_TRUE(inner.ran);
}

} // namespace test
