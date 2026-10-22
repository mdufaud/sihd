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
        TestClocks() { LoggerManager::stream(); }
        virtual ~TestClocks() { LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestClocks, test_clocks_steady_monotone)
{
    SteadyClock clock;

    const Timestamp t1 = clock.now();
    const Timestamp t2 = clock.now();
    const Timestamp t3 = clock.now();

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

    const Timestamp before = clock.now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const Timestamp after = clock.now();

    constexpr Duration five_ms_ns(5'000'000);
    EXPECT_GT(after - before, five_ms_ns);
}

TEST_F(TestClocks, test_clocks_start_stop)
{
    SteadyClock steady;
    SystemClock system;

    EXPECT_TRUE(steady.is_steady());
    EXPECT_FALSE(system.is_steady());

    EXPECT_TRUE(steady.start());
    EXPECT_GT(steady.now().get(), 0);
    EXPECT_TRUE(steady.stop());

    EXPECT_TRUE(system.start());
    EXPECT_GT(system.now().get(), 0);
    EXPECT_TRUE(system.stop());
}

} // namespace test
