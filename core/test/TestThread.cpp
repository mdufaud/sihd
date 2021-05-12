#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <sihd/core/thread.hpp>

namespace test
{
    using namespace sihd::core;
    class TestThread:   public ::testing::Test
    {
        protected:
            TestThread()
            {}
            virtual ~TestThread()
            {}
            virtual void SetUp()
            {}
            virtual void TearDown()
            {}
    };

    TEST_F(TestThread, test_Thread)
    {
        EXPECT_EQ(true, true);
    }
}
