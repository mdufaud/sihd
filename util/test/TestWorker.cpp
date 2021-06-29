#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/StepWorker.hpp>

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
        Worker worker;

        int ran = 0;
        worker.set_method([&] () -> bool
        {
            ++ran;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return true;
        });
        worker.start_worker("worker-thread");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        worker.stop_worker();
        EXPECT_EQ(ran, 1);
    }

    TEST_F(TestWorker, test_stepworker_simple)
    {
        StepWorker worker;

        int ran = 0;
        // 1 / ms
        worker.set_frequency(1000);
        worker.set_method([&] () -> bool
        {
            ++ran;
            return true;
        });
        worker.start_worker("worker-thread");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        worker.stop_worker();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        EXPECT_EQ(ran, 10);
    }

    TEST_F(TestWorker, test_stepworker_once)
    {
        StepWorker worker;

        int ran = 0;
        // 10 hz
        worker.set_frequency(0.1);
        worker.set_method([&] () -> bool
        {
            ++ran;
            return true;
        });
        worker.start_worker("worker-thread");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        worker.stop_worker();
        EXPECT_EQ(ran, 1);
    }
}
