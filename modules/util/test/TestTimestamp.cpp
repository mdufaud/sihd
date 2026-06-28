#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestTimestamp: public ::testing::Test
{
    protected:
        TestTimestamp() { sihd::util::LoggerManager::stream(); }

        virtual ~TestTimestamp() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestTimestamp, test_timestamp_modulo)
{
    Timestamp ts_mod({.hour = 1, .minute = 35, .second = 2});
    auto clocktime = ts_mod.clocktime();
    EXPECT_EQ(clocktime.hour, 1);
    EXPECT_EQ(clocktime.minute, 35);
    EXPECT_EQ(clocktime.second, 2);

    auto clocktime_mod_10 = ts_mod.modulo_min(10).clocktime();
    EXPECT_EQ(clocktime_mod_10.hour, 1);
    EXPECT_EQ(clocktime_mod_10.minute, 30);
    EXPECT_EQ(clocktime_mod_10.second, 0);

    auto clocktime_mod_60 = ts_mod.modulo_min(60).clocktime();
    EXPECT_EQ(clocktime_mod_60.hour, 1);
    EXPECT_EQ(clocktime_mod_60.minute, 0);
    EXPECT_EQ(clocktime_mod_60.second, 0);
}

TEST_F(TestTimestamp, test_timestamp_from_str)
{
    auto conversion = Timestamp::from_str("2022-02-01", "%Y-%m-%d");
    ASSERT_TRUE(conversion.has_value());

    Timestamp ts_conv = *conversion;
    auto conv_calendar = ts_conv.calendar();
    EXPECT_EQ(conv_calendar.year, 2022);
    EXPECT_EQ(conv_calendar.month, 2);
    EXPECT_EQ(conv_calendar.day, 1);
    auto conv_clocktime = ts_conv.clocktime();
    EXPECT_EQ(conv_clocktime.hour, 0);
    EXPECT_EQ(conv_clocktime.minute, 0);
    EXPECT_EQ(conv_clocktime.second, 0);

    conversion = Timestamp::from_str("2000-12-31 10-09-08", "%Y-%m-%d %H-%M-%S");
    ASSERT_TRUE(conversion.has_value());

    ts_conv = *conversion;
    conv_calendar = ts_conv.calendar();
    EXPECT_EQ(conv_calendar.year, 2000);
    EXPECT_EQ(conv_calendar.month, 12);
    EXPECT_EQ(conv_calendar.day, 31);
    conv_clocktime = ts_conv.clocktime();
    EXPECT_EQ(conv_clocktime.hour, 10);
    EXPECT_EQ(conv_clocktime.minute, 9);
    EXPECT_EQ(conv_clocktime.second, 8);

    conversion = Timestamp::from_str("2024-06-15", "%Y-%m-%d", std::locale::classic());
    ASSERT_TRUE(conversion.has_value());
    conv_calendar = conversion->calendar();
    EXPECT_EQ(conv_calendar.year, 2024);
    EXPECT_EQ(conv_calendar.month, 6);
    EXPECT_EQ(conv_calendar.day, 15);
}

TEST_F(TestTimestamp, test_timestamp_strings)
{
    Timestamp ts(1);

    SIHD_LOG(debug, "Time now: {}", ts.str());
    SIHD_LOG(debug, "Time local: {}", ts.local_str());
    SIHD_LOG(debug, "Time day: {}", ts.day_str());
    SIHD_LOG(debug, "Time sec: {}", ts.sec_str());
    SIHD_LOG(debug, "Time zone: {}", ts.zone_str());

    EXPECT_EQ(ts.str(), "1970/01/01 00:00:00");
    EXPECT_EQ(ts.sec_str(), "1970/01/01 00:00:00");
    EXPECT_EQ(ts.day_str(), "1970-01-01");
}

TEST_F(TestTimestamp, test_timestamp)
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

    // operations
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

    // format
    SIHD_LOG_DEBUG("Time offset");
    SIHD_LOG_DEBUG("{}", timestamp.timeoffset_str());
    SIHD_LOG_DEBUG("{}", timestamp.localtimeoffset_str());

    // test conversion
    auto fun = [](std::chrono::seconds sec) {
        return sec.count();
    };
    EXPECT_EQ(fun(timestamp), 4);

    // convert with clock
    std::chrono::system_clock clock;

    auto timepoint = clock.now();
    Timestamp tclock = Timestamp(timepoint);
    EXPECT_EQ(tclock, timepoint.time_since_epoch());

    // calendar and clocktime
    SIHD_LOG_DEBUG("From clocktime - flat time");
    Timestamp ts({
        .hour = 10,
        .minute = 5,
        .second = 1,
        .millisecond = 300,
    });
    Clocktime clo = ts.clocktime();
    EXPECT_EQ(clo.hour, 10);
    EXPECT_EQ(clo.minute, 5);
    EXPECT_EQ(clo.second, 1);
    EXPECT_EQ(clo.millisecond, 300);
    SIHD_LOG_DEBUG("Should be hour=10 min=5 sec=1 ms=300: {}", ts.str());

    ts = Timestamp({
        .second = 0,
        .millisecond = 1,
    });
    clo = ts.clocktime();
    EXPECT_EQ(clo.hour, 0);
    EXPECT_EQ(clo.minute, 0);
    EXPECT_EQ(clo.second, 0);
    EXPECT_EQ(clo.millisecond, 1);

    SIHD_LOG_DEBUG("From calendar - local time");
    ts = Timestamp({.day = 1, .month = 10, .year = 2022});
    Calendar cal = ts.local_calendar();
    EXPECT_EQ(cal.year, 2022);
    EXPECT_EQ(cal.month, 10);
    EXPECT_EQ(cal.day, 1);
    SIHD_LOG_DEBUG("Should be year=2022 mon=10 day=1: {}", ts.local_str());

    SIHD_LOG_DEBUG("From calendar and clocktime - local time");
    ts = Timestamp({.day = 1, .month = 10, .year = 2022},
                   {
                       .hour = 10,
                       .minute = 5,
                       .second = 1,
                   });
    cal = ts.local_calendar();
    EXPECT_EQ(cal.year, 2022);
    EXPECT_EQ(cal.month, 10);
    EXPECT_EQ(cal.day, 1);
    clo = ts.local_clocktime();
    EXPECT_EQ(clo.hour, 10);
    EXPECT_EQ(clo.minute, 5);
    EXPECT_EQ(clo.second, 1);
    SIHD_LOG_DEBUG("Should be year=2022 mon=10 day=1 hour=10 min=5 sec=1: {}", ts.local_str());

    // interval
    ts = Timestamp({
        .hour = 10,
        .minute = 5,
        .second = 59,
    });
    EXPECT_TRUE(ts.in_interval(std::chrono::hours(10), std::chrono::minutes(6)));
    EXPECT_FALSE(ts.in_interval(std::chrono::hours(9), std::chrono::minutes(6)));
}
} // namespace test
