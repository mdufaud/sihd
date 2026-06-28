#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestTime: public ::testing::Test
{
    protected:
        TestTime() { sihd::util::LoggerManager::stream(); }

        virtual ~TestTime() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestTime, test_time_leap_year)
{
    EXPECT_TRUE(time::is_leap_year(2000));
    EXPECT_TRUE(time::is_leap_year(2024));
    EXPECT_TRUE(time::is_leap_year(2400));

    EXPECT_FALSE(time::is_leap_year(1800));
    EXPECT_FALSE(time::is_leap_year(1900));
    EXPECT_FALSE(time::is_leap_year(2100));
    EXPECT_FALSE(time::is_leap_year(2022));
}

TEST_F(TestTime, test_time_duration)
{
    std::chrono::nanoseconds chrono_nano = time::to_duration<std::chrono::nanoseconds>(123);
    EXPECT_EQ(chrono_nano.count(), 123);

    std::chrono::milliseconds chrono_milli
        = time::to_duration<std::chrono::milliseconds>(time::milliseconds(456));
    EXPECT_EQ(chrono_milli.count(), 456);

    std::chrono::seconds chrono_seconds = time::to_duration<std::chrono::seconds>(time::seconds(789));
    EXPECT_EQ(chrono_seconds.count(), 789);

    std::chrono::minutes chrono_min = time::to_duration<std::chrono::minutes>(time::minutes(10));
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

    EXPECT_EQ(time::from_double_milliseconds(123.456), time::ms(123) + time::us(456));

    EXPECT_NEAR(time::to_double_milliseconds(time::ms(123) + time::us(456)), 123.456, 0.0001);
}

TEST_F(TestTime, test_time_timeval)
{
    time::UnixTime t = time::sec(55) + time::micro(300);
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
    struct tm tm = time::to_tm(time::days(3 * 365) + time::days(31) + time::days(25) + time::hours(20)
                                   + time::min(37) + time::sec(10),
                               false);
    EXPECT_EQ(tm.tm_year, 70 + 3);
    EXPECT_EQ(tm.tm_mon, 1);
    EXPECT_EQ(tm.tm_mday, 25);
    EXPECT_EQ(tm.tm_hour, 20);
    EXPECT_EQ(tm.tm_min, 37);
    EXPECT_EQ(tm.tm_sec, 10);
}
} // namespace test
