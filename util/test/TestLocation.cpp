#include <gtest/gtest.h>

#include <sihd/util/Location.hpp>

namespace test
{
using namespace sihd::util;

Location __dumb_function(Location loc = {})
{
    return loc;
}

Location my_function_location()
{
    return __dumb_function();
}

class TestLocation: public ::testing::Test
{
    protected:
        TestLocation() {}

        virtual ~TestLocation() {}

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestLocation, test_location)
{
    Location next = my_function_location();
    EXPECT_STREQ(next.file(), "util/test/TestLocation.cpp");
    EXPECT_STREQ(next.function(), "my_function_location");
    EXPECT_EQ(next.line(), 16);
}

} // namespace test
