#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/AtExit.hpp>

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
        TRACE("Message 'hello world' should be at the end of this test");
        AtExit::install();
        AtExit::add_handler(new Task([] () -> bool
        {
            std::cout << std::endl << "hello world" << std::endl << std::endl;
            return true;
        }));
    }
}
