#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/profiling.hpp>

namespace test
{
using namespace sihd::util;

class TestProfiling: public ::testing::Test
{
    protected:
        TestProfiling() { sihd::util::LoggerManager::stream(); }
        virtual ~TestProfiling() { sihd::util::LoggerManager::clear_loggers(); }
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestProfiling, test_timeit)
{
    // Just verify it compiles and runs without crash
    { Timeit t("test_function"); }
}

TEST_F(TestProfiling, test_perf)
{
    Perf p("test_perf");

    for (int i = 0; i < 10; ++i)
    {
        p.begin();
        volatile int x = 0;
        for (int j = 0; j < 100; ++j)
            x += j;
        (void)x;
        p.end();
    }

    EXPECT_EQ(p.stat().samples, 10u);
    EXPECT_GE(p.stat().min, 0);
    EXPECT_GE(p.stat().max, p.stat().min);

    p.log();
}

TEST_F(TestProfiling, test_perf_end_before_begin)
{
    Perf p("fail");
    EXPECT_THROW(p.end(), std::runtime_error);
}

} // namespace test
