#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/atexit.hpp>

namespace test
{
    LOGGER;
    using namespace sihd::util;
    class TestAtExit:   public ::testing::Test
    {
        protected:
            TestAtExit()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestAtExit()
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

    TEST_F(TestAtExit, test_atexit)
    {
        atexit::install();
        atexit::add_handler(new Task([] () -> bool {
            std::cout << std::endl << "Called after exit" << std::endl << std::endl;
            return true;
        }));
    }
}
