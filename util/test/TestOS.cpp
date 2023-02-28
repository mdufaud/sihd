#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/os.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestOs: public ::testing::Test
{
    protected:
        TestOs() { sihd::util::LoggerManager::basic(); }

        virtual ~TestOs() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestOs, test_os_backtrace)
{
    ssize_t size = os::backtrace(1);
    EXPECT_GE(size, 1);
}

TEST_F(TestOs, test_os_resources)
{
    size_t max_fds = os::max_fds();
    // in bytes
    size_t peak = os::peak_rss();
    size_t current = os::current_rss();

    SIHD_LOG(info, "Peak rss: {} mb", peak / (1024 * 1024));
    SIHD_LOG(info, "Current rss: {} mb", current / (1024 * 1024));
    SIHD_LOG(info, "Max file descriptors: {}", max_fds);

    EXPECT_TRUE(peak > 0);
    EXPECT_TRUE(current > 0);
    EXPECT_TRUE(max_fds > 2);
}
} // namespace test
