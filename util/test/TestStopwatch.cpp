#include <thread>

#include <gtest/gtest.h>

#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/time.hpp>

namespace test
{
using namespace sihd::util;

class TestStopwatch: public ::testing::Test
{
    protected:
        TestStopwatch() = default;
        virtual ~TestStopwatch() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestStopwatch, test_stopwatch_basic)
{
    Stopwatch sw;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Timestamp elapsed = sw.time();
    EXPECT_GE(elapsed, time::milli(30));
    EXPECT_LE(elapsed, time::milli(200));
}

TEST_F(TestStopwatch, test_stopwatch_reset)
{
    Stopwatch sw;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sw.reset();
    Timestamp elapsed = sw.time();
    EXPECT_LT(elapsed, time::milli(50));
}

TEST_F(TestStopwatch, test_stopwatch_monotone)
{
    Stopwatch sw;
    Timestamp t1 = sw.time();
    Timestamp t2 = sw.time();
    EXPECT_LE(t1, t2);
}

} // namespace test
