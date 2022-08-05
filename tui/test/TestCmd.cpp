#include <gtest/gtest.h>
#include <iostream>

namespace test
{
    class TestCmd: public ::testing::Test
    {
        protected:
            TestCmd()
            {
            }

            virtual ~TestCmd()
            {
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestCmd, test_tui_cmd)
    {
    }
}