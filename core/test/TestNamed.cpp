#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <sihd/core/Named.hpp>

namespace test
{
    using namespace sihd::core;
    class TestNamed:   public ::testing::Test
    {
        protected:
            TestNamed()
            {}
            virtual ~TestNamed()
            {}
            virtual void SetUp()
            {}
            virtual void TearDown()
            {}
    };

    TEST_F(TestNamed, test_Named)
    {
        EXPECT_EQ(true, true);
    }
}
