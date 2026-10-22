#include <chrono>

#include <gtest/gtest.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/Semaphore.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::sys;

class TestSemaphore: public ::testing::Test
{
    protected:
        TestSemaphore() { LoggerManager::stream(); }

        virtual ~TestSemaphore() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!Semaphore::supported)
            {
                GTEST_SKIP() << "named semaphore (sem_open) not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestSemaphore, test_named_semaphore)
{
    Semaphore first;
    ASSERT_TRUE(first.create("/sihd_named_semaphore", 2));

    Semaphore sem(std::move(first));
    EXPECT_TRUE(sem.creator());
    EXPECT_EQ(sem.name(), "/sihd_named_semaphore");

    EXPECT_TRUE(sem.try_wait());
    EXPECT_TRUE(sem.try_wait());
    EXPECT_FALSE(sem.try_wait());

    EXPECT_TRUE(sem.post());
    EXPECT_TRUE(sem.try_wait());
    EXPECT_FALSE(sem.try_wait());

    if constexpr (!build::is_windows)
    {
        auto value = sem.value();
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(*value, 0);
    }

    // double create is exclusive on posix, windows opens the existing object
    Semaphore attach;
    if constexpr (build::is_windows)
    {
        ASSERT_TRUE(attach.create("/sihd_named_semaphore", 0));
        EXPECT_FALSE(attach.creator());
    }
    else
    {
        EXPECT_FALSE(attach.create("/sihd_named_semaphore", 0));
        ASSERT_TRUE(attach.attach("/sihd_named_semaphore"));
        EXPECT_FALSE(attach.creator());
    }

    EXPECT_TRUE(attach.post());
    EXPECT_TRUE(attach.try_wait());

    EXPECT_TRUE(sem.clear());
    EXPECT_TRUE(attach.clear());

    Semaphore gone;
    EXPECT_FALSE(gone.attach("/sihd_named_semaphore"));
}

TEST_F(TestSemaphore, test_named_semaphore_timeout)
{
    Semaphore sem;
    ASSERT_TRUE(sem.create("/sihd_named_semaphore_timeout", 0));

    EXPECT_FALSE(sem.try_wait());
    EXPECT_FALSE(sem.wait_for(std::chrono::milliseconds(50)));
    EXPECT_FALSE(sem.wait_for(Duration(time::milli(50))));

    EXPECT_TRUE(sem.post());
    EXPECT_TRUE(sem.wait());
    EXPECT_TRUE(sem.clear());
}

TEST_F(TestSemaphore, test_named_semaphore_process)
{
    Semaphore sem;
    ASSERT_TRUE(sem.create("/sihd_named_semaphore_process", 0));

    Process wait_proc({helper_path(), "sem-wait", "/sihd_named_semaphore_process"});
    (void)wait_proc.execute();

    ASSERT_TRUE(sem.post());
    (void)wait_proc.wait_any();
    EXPECT_TRUE(wait_proc.return_code() & (1 << 1));
    EXPECT_TRUE(wait_proc.return_code() & (1 << 2));

    Process post_proc({helper_path(), "sem-post", "/sihd_named_semaphore_process"});
    (void)post_proc.execute();

    EXPECT_TRUE(sem.wait_for(std::chrono::seconds(5)));
    (void)post_proc.wait_any();
    EXPECT_TRUE(post_proc.return_code() & (1 << 1));
    EXPECT_TRUE(post_proc.return_code() & (1 << 2));

    EXPECT_TRUE(sem.clear());
}

} // namespace test
