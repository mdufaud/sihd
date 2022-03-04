#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Value.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    using namespace sihd::util;
    class TestValue: public ::testing::Test
    {
        protected:
            TestValue()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::Files::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "util",
                    "value"
                });
                _cwd = sihd::util::OS::get_cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
            }

            virtual ~TestValue()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _cwd;
            std::string _base_test_dir;
    };

    TEST_F(TestValue, test_value_float)
    {
        Value val;

        val.set(3.124);
        EXPECT_TRUE(val > 3.123);
        EXPECT_TRUE(val == 3.124);
        EXPECT_TRUE(val < 3.125);
    }

    TEST_F(TestValue, test_value)
    {
        int8_t test8 = 8;
        int16_t test16 = 16;
        int32_t test32 = 32;
        int64_t test64 = 64;
        float testf = 3.14;
        double testd = 6.28;

        Value val;
        val.set(test8);
        EXPECT_TRUE(val == test8);
        EXPECT_TRUE(val < test16);
        EXPECT_TRUE(val <= test16);
        EXPECT_FALSE(val > test16);
        EXPECT_FALSE(val >= test16);
        val.set(test16);
        EXPECT_TRUE(val == test16);
        val.set(test32);
        EXPECT_TRUE(val == test32);
        val.set(test64);
        EXPECT_TRUE(val == test64);
        val.set(testf);
        EXPECT_TRUE(val == testf);
        EXPECT_TRUE(val < 4.0);
        val.set(testd);
        EXPECT_TRUE(val == testd);
        EXPECT_TRUE(val > 6.0);
    }

}