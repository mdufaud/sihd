#include <gtest/gtest.h>

#include <sihd/util/Duration.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestDuration: public ::testing::Test
{
    protected:
        TestDuration() { sihd::util::LoggerManager::stream(); }

        virtual ~TestDuration() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDuration, test_duration_class)
{
    // construction shares Timestamp's ctors
    Duration from_chrono(std::chrono::seconds(2));
    EXPECT_EQ(from_chrono.nanoseconds(), 2E9);
    EXPECT_EQ(from_chrono.seconds(), 2);

    Duration from_unixtime(time::milliseconds(456));
    EXPECT_EQ(from_unixtime.milliseconds(), 456);

    Duration full(time::hours(1) + time::minutes(2) + time::seconds(3));
    EXPECT_EQ(full.hours(), 1);
    EXPECT_EQ(full.minutes(), 62);
    EXPECT_EQ(full.seconds(), 3723);
    EXPECT_EQ(full.str(), "+1h:2m:3s:0ms:0us");
}

TEST_F(TestDuration, test_duration_strict_algebra)
{
    Timestamp t1(time::seconds(10));
    Timestamp t2(time::seconds(3));

    // Timestamp - Timestamp -> Duration (elapsed)
    static_assert(std::same_as<decltype(t1 - t2), Duration>);
    Duration elapsed = t1 - t2;
    EXPECT_EQ(elapsed.seconds(), 7);

    // Timestamp +/- Duration -> Timestamp
    static_assert(std::same_as<decltype(t2 + elapsed), Timestamp>);
    static_assert(std::same_as<decltype(elapsed + t2), Timestamp>);
    static_assert(std::same_as<decltype(t1 - elapsed), Timestamp>);
    EXPECT_EQ((t2 + elapsed).seconds(), 10);
    EXPECT_EQ((elapsed + t2).seconds(), 10);
    EXPECT_EQ((t1 - elapsed).seconds(), 3);

    // Duration +/- Duration -> Duration
    static_assert(std::same_as<decltype(elapsed + elapsed), Duration>);
    EXPECT_EQ((Duration(time::seconds(2)) + Duration(time::seconds(3))).seconds(), 5);
    EXPECT_EQ((Duration(time::seconds(5)) - Duration(time::seconds(2))).seconds(), 3);

    // Duration * / scalar -> Duration ; Duration % Duration -> Duration
    static_assert(std::same_as<decltype(elapsed * 2), Duration>);
    EXPECT_EQ((Duration(time::seconds(2)) * 3).seconds(), 6);
    EXPECT_EQ((Duration(time::seconds(10)) / 2).seconds(), 5);
    EXPECT_EQ((Duration(time::seconds(7)) % Duration(time::seconds(3))).seconds(), 1);
}
} // namespace test
