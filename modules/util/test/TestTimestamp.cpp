#include <limits>

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
        TestTimestamp() { LoggerManager::stream(); }

        virtual ~TestTimestamp() { LoggerManager::clear_loggers(); }

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
    EXPECT_THROW(ts_mod.modulo_min(0), std::invalid_argument);
}

TEST_F(TestTimestamp, test_timestamp_before_epoch)
{
    Timestamp timestamp(std::chrono::milliseconds(-500));
    EXPECT_EQ(timestamp.format("%Y-%m-%d %H:%M:%S"), "1969-12-31 23:59:59");
    EXPECT_EQ(timestamp.calendar().year, 1969);
    EXPECT_EQ(timestamp.clocktime().millisecond, 500);
}

TEST_F(TestTimestamp, test_timestamp_utc_range)
{
    Timestamp maximum(std::numeric_limits<int64_t>::max());
    Timestamp minimum(std::numeric_limits<int64_t>::min());
    EXPECT_EQ(maximum.format("%Y-%m-%d %H:%M:%S"), "2262-04-11 23:47:16");
    EXPECT_EQ(minimum.format("%Y-%m-%d %H:%M:%S"), "1677-09-21 00:12:43");
    if constexpr (sizeof(time_t) < sizeof(int64_t))
    {
        EXPECT_THROW(maximum.local_tm(), std::out_of_range);
        EXPECT_THROW(Timestamp(Calendar {.day = 1, .month = 1, .year = 2262}), std::out_of_range);
    }
    EXPECT_THROW(Timestamp(Calendar {.day = 1, .month = 1, .year = 3000}), std::out_of_range);
}

TEST_F(TestTimestamp, test_timestamp_calendar_milliseconds)
{
    Timestamp timestamp(Calendar {.day = 1, .month = 1, .year = 2020}, Clocktime {.millisecond = 123});
    EXPECT_EQ(timestamp.local_clocktime().millisecond, 123);
}

TEST_F(TestTimestamp, test_timestamp_from_str)
{
    EXPECT_FALSE(Timestamp::from_str("3000-01-01", "%Y-%m-%d").has_value());
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    const auto fractional = Timestamp::from_str("2022-02-01 00:00:00.123456789", "%Y-%m-%d %H:%M:%S");
    ASSERT_TRUE(fractional.has_value());
    EXPECT_EQ(fractional->get() % time::seconds(1), 123456789);
    const auto minimum = Timestamp::from_str("1677-09-21 00:12:43.145224192", "%Y-%m-%d %H:%M:%S");
    const auto maximum = Timestamp::from_str("2262-04-11 23:47:16.854775807", "%Y-%m-%d %H:%M:%S");
    ASSERT_TRUE(minimum.has_value());
    ASSERT_TRUE(maximum.has_value());
    EXPECT_EQ(minimum->get(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(maximum->get(), std::numeric_limits<int64_t>::max());
    EXPECT_FALSE(Timestamp::from_str("1677-09-21 00:12:43.145224191", "%Y-%m-%d %H:%M:%S").has_value());
    EXPECT_FALSE(Timestamp::from_str("2262-04-11 23:47:16.854775808", "%Y-%m-%d %H:%M:%S").has_value());
#endif
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

    EXPECT_EQ(timestamp, time::seconds(2));

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

    SIHD_LOG(debug, "Time offset");
    SIHD_LOG(debug, "{}", timestamp.timeoffset_str());
    SIHD_LOG(debug, "{}", timestamp.localtimeoffset_str());

    auto fun = [](std::chrono::seconds sec) {
        return sec.count();
    };
    EXPECT_EQ(fun(timestamp), 4);

    std::chrono::system_clock clock;

    auto timepoint = clock.now();
    Timestamp tclock = Timestamp(timepoint);
    EXPECT_EQ(tclock, timepoint.time_since_epoch());

    SIHD_LOG(debug, "From clocktime - flat time");
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
    SIHD_LOG(debug, "Should be hour=10 min=5 sec=1 ms=300: {}", ts.str());

    ts = Timestamp({
        .second = 0,
        .millisecond = 1,
    });
    clo = ts.clocktime();
    EXPECT_EQ(clo.hour, 0);
    EXPECT_EQ(clo.minute, 0);
    EXPECT_EQ(clo.second, 0);
    EXPECT_EQ(clo.millisecond, 1);

    SIHD_LOG(debug, "From calendar - local time");
    ts = Timestamp({.day = 1, .month = 10, .year = 2022});
    Calendar cal = ts.local_calendar();
    EXPECT_EQ(cal.year, 2022);
    EXPECT_EQ(cal.month, 10);
    EXPECT_EQ(cal.day, 1);
    SIHD_LOG(debug, "Should be year=2022 mon=10 day=1: {}", ts.local_str());

    SIHD_LOG(debug, "From calendar and clocktime - local time");
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
    SIHD_LOG(debug, "Should be year=2022 mon=10 day=1 hour=10 min=5 sec=1: {}", ts.local_str());

    ts = Timestamp({
        .hour = 10,
        .minute = 5,
        .second = 59,
    });
    EXPECT_TRUE(ts.in_interval(std::chrono::hours(10), std::chrono::minutes(6)));
    EXPECT_FALSE(ts.in_interval(std::chrono::hours(9), std::chrono::minutes(6)));
}
} // namespace test
