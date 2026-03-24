#include <gtest/gtest.h>

import sihd.util.test.greeting;

TEST(TestCppModules, test_cpp_module_greeting)
{
    EXPECT_EQ(sihd::util::test::module_greeting_value(), 42);
}