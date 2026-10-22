#include <array>
#include <cstdlib>

#include <gtest/gtest.h>

#include <sihd/sys/File.hpp>
#include <sihd/sys/MemLock.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/build.hpp>

#include "test_helper.hpp"

#if defined(__SIHD_WINDOWS__)
# include <winsock2.h>

# include <fcntl.h>
# include <io.h>
#else
# include <unistd.h>
#endif

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestOs: public ::testing::Test
{
    protected:
        TestOs() { LoggerManager::stream(); }

        virtual ~TestOs() { LoggerManager::clear_loggers(); }

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
    SystemClock system_clock;
    const Timestamp boot = sihd::sys::os::boot_time();
    const Timestamp now(system_clock.now());
    EXPECT_GT(boot, Timestamp(0));
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

TEST_F(TestOs, test_os_fd_type)
{
    File file;
    ASSERT_TRUE(file.open_tmpfile());

    auto type_res = os::fd_type(file.fd());
    ASSERT_TRUE(type_res.has_value());
    EXPECT_EQ(type_res.value(), os::FdType::regular);
    EXPECT_TRUE(os::is_regular(file.fd()));
    EXPECT_FALSE(os::is_tty(file.fd()));
    EXPECT_FALSE(os::is_pipe(file.fd()));
    EXPECT_FALSE(os::is_socket(file.fd()));

    EXPECT_FALSE(os::fd_type(-1).has_value());
    EXPECT_FALSE(os::is_tty(-1));
}

TEST_F(TestOs, test_os_fd_dup)
{
    File file;
    ASSERT_TRUE(file.open_tmpfile());

    auto dup_res = os::dup(file.fd());
    ASSERT_TRUE(dup_res.has_value());
    const int dup_fd = dup_res.value();
    EXPECT_NE(dup_fd, file.fd());

    // both descriptors share the offset: write through one, read back from the other
    ASSERT_EQ(write_file_fd(dup_fd, "dup", 3), 3);
    ASSERT_EQ(seek_file_fd(file.fd(), 0, SEEK_SET), 0);
    char buf[4] = {};
    ASSERT_EQ(read_file_fd(file.fd(), buf, 3), 3);
    EXPECT_STREQ(buf, "dup");

    auto dup2_res = os::dup2(file.fd(), dup_fd);
    ASSERT_TRUE(dup2_res.has_value());
    EXPECT_EQ(dup2_res.value(), dup_fd);

    EXPECT_FALSE(os::dup(-1).has_value());
    EXPECT_FALSE(os::dup2(-1, 42).has_value());

    close_fd(dup_fd);
}

TEST_F(TestOs, test_os_fd_pair)
{
    std::array<int, 2> fds = {-1, -1};
    ASSERT_TRUE(make_fd_pair(fds));

#if defined(__SIHD_WINDOWS__)
    EXPECT_TRUE(os::is_socket(fds[0]));
    EXPECT_TRUE(os::is_socket(fds[1]));
#else
    EXPECT_TRUE(os::is_pipe(fds[0]));
    EXPECT_TRUE(os::is_pipe(fds[1]));
#endif
    EXPECT_FALSE(os::is_regular(fds[0]));
    EXPECT_FALSE(os::is_tty(fds[0]));

    ASSERT_TRUE(os::set_nonblocking(fds[0]).has_value());

    ASSERT_EQ(write_fd(fds[1], "pair", 4), 4);

    auto avail_res = os::bytes_available(fds[0]);
    ASSERT_TRUE(avail_res.has_value());
    EXPECT_EQ(avail_res.value(), (size_t)4);

    char buf[8] = {};
    ASSERT_EQ(read_fd(fds[0], buf, sizeof(buf)), 4);
    EXPECT_STREQ(buf, "pair");

    auto avail_after = os::bytes_available(fds[0]);
    ASSERT_TRUE(avail_after.has_value());
    EXPECT_EQ(avail_after.value(), (size_t)0);

    close_fd(fds[0]);
    close_fd(fds[1]);
}

#if defined(__SIHD_WINDOWS__)
TEST_F(TestOs, test_os_fd_windows_pipe)
{
    int fds[2];
    ASSERT_EQ(_pipe(fds, 4096, _O_BINARY), 0);

    EXPECT_TRUE(os::is_pipe(fds[0]));
    EXPECT_FALSE(os::set_nonblocking(fds[0]).has_value());

    ASSERT_EQ(write_file_fd(fds[1], "pipe", 4), 4);
    auto avail_res = os::bytes_available(fds[0]);
    ASSERT_TRUE(avail_res.has_value());
    EXPECT_EQ(avail_res.value(), (size_t)4);

    _close(fds[0]);
    _close(fds[1]);
}
#endif
} // namespace test
