#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>

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

    TEST_F(TestWorker, test_worker)
    {
        Worker worker("worker");

        int ran = 0;
        // 10 hz
        worker.set_frequency(1000);
        worker.set_method([&] (time_t delta) -> bool
        {
            TRACE(time::to_micro(delta) << " us");
            ++ran;
            return true;
        });
        worker.start_thread();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        worker.stop_thread();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        EXPECT_EQ(ran, 10);
    }
}
