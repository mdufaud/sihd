#include <csignal>
#include <filesystem>
#include <fstream>
#include <wait_for.hpp>

#include <gtest/gtest.h>

#include <sihd/sys/Daemon.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "test_helper.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestDaemon: public ::testing::Test
{
    protected:
        TestDaemon() { LoggerManager::stream(); }

        virtual ~TestDaemon() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestDaemon, test_daemon_unknown_user_group_refused)
{
    Daemon daemon("daemon");
    EXPECT_FALSE(daemon.set_user("no-such-user-sihd"));
    EXPECT_FALSE(daemon.set_group("no-such-group-sihd"));
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string pid_file = tmp_dir.path() + "/daemon.pid";
    ASSERT_TRUE(daemon.set_pid_file_path(pid_file));
    ASSERT_TRUE(daemon.set_working_dir_path(tmp_dir.path()));
    EXPECT_EQ(daemon.pid_file(), pid_file);
    EXPECT_EQ(daemon.working_dir(), tmp_dir.path());
}

#if defined(__SIHD_LINUX__)
TEST_F(TestDaemon, test_daemon_resolves_user_group)
{
    if constexpr (build::is_statically_linked)
        GTEST_SKIP() << "the daemon user/group conf resolves through NSS, unavailable in static binaries";
    Daemon daemon("daemon");
    ASSERT_TRUE(daemon.set_user("root"));
    EXPECT_TRUE(daemon.user_id().valid());
    EXPECT_EQ(daemon.user_id().native(), 0u);
    ASSERT_TRUE(daemon.set_group("root"));
    EXPECT_TRUE(daemon.group_id().valid());
}
#endif

#if defined(__SIHD_LINUX__)
TEST_F(TestDaemon, test_daemon_pid_file_lifecycle)
{
    if (os::is_run_by_valgrind())
        GTEST_SKIP() << "fork under valgrind";
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string pid_file = tmp_dir.path() + "/daemon.pid";
    const std::string work_dir = tmp_dir.path() + "/work";
    ASSERT_TRUE(std::filesystem::create_directory(work_dir));

    Process daemon_proc({helper_path(), "daemonize", pid_file, work_dir, "1"});
    ASSERT_TRUE(daemon_proc.execute().has_value());
    ASSERT_TRUE(daemon_proc.wait_any().value_or(false));

    int daemon_pid = -1;
    ASSERT_TRUE(wait_for([&] {
        std::ifstream stream(pid_file);
        return static_cast<bool>(stream >> daemon_pid) && daemon_pid > 0;
    }));
    EXPECT_EQ(::kill(daemon_pid, 0), 0);

    EXPECT_TRUE(wait_for([&] { return fs::exists(pid_file) == false; }, std::chrono::seconds(5)));
}
#endif

} // namespace test
