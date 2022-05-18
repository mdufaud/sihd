#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/time.hpp>
#include <sihd/util/Timestamp.hpp>

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

    TEST_F(TestTime, test_time_timestamp)
    {
        Timestamp timestamp(std::chrono::seconds(2));

        EXPECT_EQ(timestamp.get(), time::seconds(2));

        std::chrono::seconds s = timestamp;
        EXPECT_EQ(s.count(), 2);

        // auto converted to time_t
        EXPECT_EQ(timestamp, time::seconds(2));

        // comparisons
        EXPECT_EQ(timestamp, Timestamp(timestamp));
        EXPECT_EQ(timestamp, timestamp);
        EXPECT_EQ(timestamp, timestamp.get());
        EXPECT_LE(timestamp, Timestamp(timestamp + 1));
        EXPECT_LE(timestamp, timestamp + 1);
        EXPECT_GT(timestamp, Timestamp(timestamp - 1));
        EXPECT_GT(timestamp, timestamp - 1);

        timestamp += time::seconds(1);
        timestamp += std::chrono::seconds(1);
        EXPECT_EQ(timestamp - std::chrono::seconds(1), std::chrono::seconds(3));

        EXPECT_EQ(timestamp.nanoseconds(), 4E9);
        EXPECT_EQ(timestamp.microseconds(), 4E6);
        EXPECT_EQ(timestamp.milliseconds(), 4E3);
        EXPECT_EQ(timestamp.seconds(), 4);
        EXPECT_EQ(timestamp.minutes(), 0);
        EXPECT_EQ(timestamp.hours(), 0);
        EXPECT_EQ(timestamp.days(), 0);

        SIHD_LOG_DEBUG(timestamp.gmtime_str());
        SIHD_LOG_DEBUG(timestamp.localtime_str());

        auto fun = [] (std::chrono::seconds sec) { return sec.count(); };
        EXPECT_EQ(fun(timestamp), 4);
    }

    TEST_F(TestTime, test_time_duration)
    {
        std::chrono::nanoseconds chrono_nano = time::to_duration<std::nano>(123);
        EXPECT_EQ(chrono_nano.count(), 123);

        std::chrono::milliseconds chrono_milli = time::to_duration<std::milli>(time::milliseconds(456));
        EXPECT_EQ(chrono_milli.count(), 456);

        std::chrono::seconds chrono_seconds = time::to_duration<std::ratio<1>>(time::seconds(789));
        EXPECT_EQ(chrono_seconds.count(), 789);

        std::chrono::minutes chrono_min = time::to_duration<std::ratio<60>>(time::minutes(10));
        EXPECT_EQ(chrono_min.count(), 10);

        EXPECT_EQ(time::duration(std::chrono::nanoseconds(2)), 2);
        EXPECT_EQ(time::duration(std::chrono::microseconds(2)), time::microseconds(2));
        EXPECT_EQ(time::duration(std::chrono::milliseconds(2)), time::milliseconds(2));
        EXPECT_EQ(time::duration(std::chrono::seconds(2)), time::seconds(2));
        EXPECT_EQ(time::duration(std::chrono::minutes(2)), time::minutes(2));
        EXPECT_EQ(time::duration(std::chrono::hours(2)), time::hours(2));
    }

    TEST_F(TestTime, test_time_freq)
    {
        EXPECT_DOUBLE_EQ(time::freq(10), time::milli(100));
        EXPECT_DOUBLE_EQ(time::to_hz(time::milli(100)), 10);
    }

    TEST_F(TestTime, test_time_double)
    {
        double time_dbl = time::to_double(time::sec(1) + time::milli(23));
        EXPECT_DOUBLE_EQ(time_dbl, 1.023);

        struct timeval tv = time::double_to_tv(time_dbl);
        EXPECT_EQ(tv.tv_sec, 1);
        EXPECT_NEAR(tv.tv_usec, (time_t)23E3, 2);

        double time_dbl_from_tv = time::tv_to_double(tv);
        EXPECT_NEAR(time_dbl, time_dbl_from_tv, 0.0001);

        EXPECT_EQ(time::from_double(0.001), time::ms(1));
        EXPECT_EQ(time::from_double(1.324999), time::sec(1) + time::ms(324) + time::micro(999));
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
        struct tm *tm = time::to_tm(time::days(3 * 365) + time::days(31) + time::days(25)
                                        + time::hours(20) + time::min(37) + time::sec(10),
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