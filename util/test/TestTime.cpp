#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/time.hpp>

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::util;
    class TestTime: public ::testing::Test
    {
        protected:
            TestTime()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestTime()
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

    TEST_F(TestTime, test_time_double)
    {
        double time_dbl = time::to_double(time::sec(1) + time::milli(23));
        EXPECT_DOUBLE_EQ(time_dbl, 1.023);

        struct timeval tv = time::double_to_tv(time_dbl);
        EXPECT_EQ(tv.tv_sec, 1);
        EXPECT_NEAR(tv.tv_usec, (time_t)23E3, 2);

        double time_dbl_from_tv = time::tv_to_double(tv);
        EXPECT_NEAR(time_dbl, time_dbl_from_tv, 0.0001);
    }

    TEST_F(TestTime, test_time_timeval)
    {
        time_t t = time::sec(55) + time::micro(300);
        struct timeval tv = time::to_tv(time::to_micro(t));
        EXPECT_EQ(tv.tv_sec, 55);
        EXPECT_EQ(tv.tv_usec, 300);
        EXPECT_EQ(time::tv(tv), t);

        struct timeval nano_tv = time::to_nano_tv(350);
        EXPECT_EQ(nano_tv.tv_sec, 0);
        EXPECT_EQ(nano_tv.tv_usec, 350);
        EXPECT_EQ(time::nano_tv(nano_tv), 350);
    }

    TEST_F(TestTime, test_time_tm)
    {
        struct tm *tm = time::to_tm(time::day(3 * 365) + time::day(31) + time::day(25)
                                        + time::hour(20) + time::min(37) + time::sec(10),
                                        false);
        EXPECT_NE(tm, nullptr);
        if (tm)
        {
            EXPECT_EQ(tm->tm_year, 70 + 3);
            EXPECT_EQ(tm->tm_mon, 1);
            EXPECT_EQ(tm->tm_mday, 25);
            EXPECT_EQ(tm->tm_hour, 20);
            EXPECT_EQ(tm->tm_min, 37);
            EXPECT_EQ(tm->tm_sec, 10);
        }
    }
}