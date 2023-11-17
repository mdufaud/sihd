#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/os.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestWorker: public ::testing::Test
{
    protected:
        TestWorker() { sihd::util::LoggerManager::basic(); }

        virtual ~TestWorker() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestWorker, test_worker_simple)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    int ran = 0;
    Runnable runnable([&]() -> bool {
        ++ran;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return true;
    });
    Worker worker(&runnable);
    EXPECT_TRUE(worker.start_sync_worker("worker-thread"));
    EXPECT_TRUE(worker.stop_worker());
    EXPECT_EQ(ran, 1);
    EXPECT_TRUE(worker.start_sync_worker("worker-thread"));
    EXPECT_TRUE(worker.stop_worker());
    EXPECT_EQ(ran, 2);
}

TEST_F(TestWorker, test_stepworker_multiple)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    int ran = 0;
    Runnable runnable([&]() -> bool {
        ++ran;
        return true;
    });
    StepWorker worker(&runnable);
    // 1 / ms
    EXPECT_TRUE(worker.set_frequency(500));
    EXPECT_TRUE(worker.start_sync_worker("stepworker-thread"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(worker.stop_worker());
    int really_ran = ran;
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
    EXPECT_GE(really_ran, 2);
    EXPECT_EQ(ran, really_ran);
}

TEST_F(TestWorker, test_stepworker_once)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    int ran = 0;
    Runnable runnable([&]() -> bool {
        ++ran;
        return true;
    });
    StepWorker worker(&runnable);
    // 10 hz
    EXPECT_TRUE(worker.set_frequency(0.1));
    EXPECT_TRUE(worker.start_sync_worker("stepworker-thread"));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_TRUE(worker.stop_worker());
    EXPECT_EQ(ran, 1);
}
} // namespace test
