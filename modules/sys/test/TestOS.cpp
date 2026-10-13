#include <cstdlib>

#include <gtest/gtest.h>

#include <sihd/sys/MemLock.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/build.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestOs: public ::testing::Test
{
    protected:
        TestOs() { sihd::util::LoggerManager::stream(); }

        virtual ~TestOs() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestOs, test_os_backtrace)
{
    ssize_t size = sihd::sys::os::backtrace(1);
    if constexpr (build::is_gnu || build::is_windows)
        EXPECT_GE(size, 1);
    else
        // musl/android/emscripten ship no backtrace() - os::backtrace is a no-op stub
        EXPECT_EQ(size, 0);
}

TEST_F(TestOs, test_os_resources)
{
    auto max_fds = sihd::sys::os::max_fds();
    ASSERT_TRUE(max_fds.has_value());
    // in bytes
    auto peak = sihd::sys::os::peak_rss();
    ASSERT_TRUE(peak.has_value());
    size_t current = sihd::sys::os::current_rss();

    SIHD_LOG(info, "Peak rss: {} mb", *peak / (1024 * 1024));
    SIHD_LOG(info, "Current rss: {} mb", current / (1024 * 1024));
    SIHD_LOG(info, "Max file descriptors: {}", *max_fds);

    EXPECT_TRUE(*peak > 0);
    EXPECT_TRUE(current > 0);
    EXPECT_TRUE(*max_fds > 2);
}

TEST_F(TestOs, test_os_exists_in_path)
{
    // wine inherits the unix PATH: binary lookups are meaningless there
    if (os::is_run_by_wine())
        GTEST_SKIP() << "exists_in_path under wine";
#if defined(__SIHD_WINDOWS__)
    EXPECT_TRUE(sihd::sys::os::exists_in_path("cmd.exe"));
#else
    EXPECT_TRUE(sihd::sys::os::exists_in_path("sh"));
#endif
    EXPECT_FALSE(sihd::sys::os::exists_in_path("no-such-binary-sihd-xyz"));
}

TEST_F(TestOs, test_os_boot_time)
{
    sihd::util::SystemClock system_clock;
    const sihd::util::Timestamp boot = sihd::sys::os::boot_time();
    const sihd::util::Timestamp now(system_clock.now());
    EXPECT_GT(boot, sihd::util::Timestamp(0));
    EXPECT_LT(boot, now);
}

TEST_F(TestOs, test_os_mem_lock)
{
    if constexpr (!sihd::sys::os::mem_lock_supported)
    {
        GTEST_SKIP() << "mem lock not supported on this platform";
    }

    char buffer[4096] = {};
    EXPECT_TRUE(sihd::sys::os::mem_lock(buffer, sizeof(buffer)).has_value());
    EXPECT_TRUE(sihd::sys::os::mem_unlock(buffer, sizeof(buffer)).has_value());
}

TEST_F(TestOs, test_os_mem_lock_guard)
{
    if constexpr (!sihd::sys::os::mem_lock_supported)
    {
        GTEST_SKIP() << "mem lock not supported on this platform";
    }

    char buffer[4096] = {};
    MemLock lock(buffer, sizeof(buffer));
    EXPECT_TRUE(lock.locked());
    lock.unlock();
    EXPECT_FALSE(lock.locked());
}
} // namespace test
