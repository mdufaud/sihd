#include <stdexcept>

#include <gtest/gtest.h>

#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Task.hpp>

namespace test
{
using namespace sihd::util;

class TestTask: public ::testing::Test
{
    protected:
        TestTask() = default;
        virtual ~TestTask() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestTask, test_task_options_stored)
{
    TaskOptions opts;
    opts.run_at = 0;
    opts.run_in = 100;
    opts.reschedule_time = 50;

    Task t(opts);
    EXPECT_EQ(t.run_at, (time_t)0);
    EXPECT_EQ(t.run_in, (time_t)100);
    EXPECT_EQ(t.reschedule_time, (time_t)50);
}

TEST_F(TestTask, test_task_run_at_and_run_in_throws)
{
    TaskOptions opts;
    opts.run_at = 100;
    opts.run_in = 100;

    EXPECT_THROW(Task t(opts), std::logic_error);
}

TEST_F(TestTask, test_task_set_method)
{
    bool called = false;
    Task t([&called]() -> bool {
        called = true;
        return true;
    });

    EXPECT_TRUE(t.run());
    EXPECT_TRUE(called);
}

TEST_F(TestTask, test_task_set_runnable)
{
    struct SimpleRunnable: public IRunnable
    {
            bool ran = false;
            bool run() override
            {
                ran = true;
                return true;
            }
    };

    SimpleRunnable inner;
    Task t(&inner);

    EXPECT_TRUE(t.run());
    EXPECT_TRUE(inner.ran);
}

TEST_F(TestTask, test_task_no_callable_returns_false)
{
    Task t;
    EXPECT_FALSE(t.run());
}

} // namespace test
