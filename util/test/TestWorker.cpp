#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/StepWorker.hpp>
#include <sihd/util/Task.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestWorker:   public ::testing::Test
    {
        protected:
            TestWorker()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestWorker()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestWorker, test_worker_simple)
    {
        int ran = 0;
        Task task([&] () -> bool
        {
            ++ran;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return true;
        });
        Worker worker(&task);
        EXPECT_TRUE(worker.start_sync_worker("worker-thread"));
        EXPECT_TRUE(worker.stop_worker());
        EXPECT_EQ(ran, 1);
    }

    TEST_F(TestWorker, test_stepworker_simple)
    {
        int ran = 0;
        Task task([&] () -> bool
        {
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
        EXPECT_EQ(ran, 10);
    }

    TEST_F(TestWorker, test_stepworker_once)
    {
        int ran = 0;
        Task task([&] () -> bool
        {
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
}
