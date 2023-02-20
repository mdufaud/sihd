#include <gtest/gtest.h>
#include <sihd/util/Logger.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Task.hpp>
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
    Task task([&]() -> bool {
        ++ran;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return true;
    });
    Worker worker(&task);
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
    Task task([&]() -> bool {
        ++ran;
        return true;
    });
    StepWorker worker(&task);
    // 1 / ms
    EXPECT_TRUE(worker.set_frequency(1000));
    EXPECT_TRUE(worker.start_sync_worker("stepworker-thread"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(worker.stop_worker());
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_NEAR(ran, 10, 1);
}

TEST_F(TestWorker, test_stepworker_once)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "Buggy with valgrind";
    int ran = 0;
    Task task([&]() -> bool {
        ++ran;
        return true;
    });
    StepWorker worker(&task);
    // 10 hz
    EXPECT_TRUE(worker.set_frequency(0.1));
    EXPECT_TRUE(worker.start_sync_worker("stepworker-thread"));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_TRUE(worker.stop_worker());
    EXPECT_EQ(ran, 1);
}
} // namespace test
