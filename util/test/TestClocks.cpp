#include <thread>

#include <gtest/gtest.h>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{

SIHD_NEW_LOGGER("test");
using namespace sihd::util;

class TestClocks: public ::testing::Test
{
    protected:
        TestClocks() { sihd::util::LoggerManager::stream(); }
        virtual ~TestClocks() { sihd::util::LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestClocks, test_clocks_steady_monotone)
{
    SteadyClock clock;

    const time_t t1 = clock.now();
    const time_t t2 = clock.now();
    const time_t t3 = clock.now();

    EXPECT_LE(t1, t2);
    EXPECT_LE(t2, t3);
    EXPECT_GT(t1, 0);
}

TEST_F(TestClocks, test_clocks_is_steady)
{
    SteadyClock steady;
    SystemClock system;

    EXPECT_TRUE(steady.is_steady());
    EXPECT_FALSE(system.is_steady());
}

TEST_F(TestClocks, test_clocks_elapsed)
{
    SteadyClock clock;

    const time_t before = clock.now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const time_t after = clock.now();

    // elapsed should be at least 5ms in nanoseconds
    constexpr time_t five_ms_ns = 5'000'000;
    EXPECT_GT(after - before, five_ms_ns);
}

} // namespace test
