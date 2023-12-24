#include <gtest/gtest.h>
#include <sihd/ssh/SshScp.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::ssh;
using namespace sihd::util;
class TestSshScp: public ::testing::Test
{
    protected:
        TestSshScp() { sihd::util::LoggerManager::basic(); }

        virtual ~TestSshScp() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        TmpDir _tmp_dir;
};

TEST_F(TestSshScp, test_sshscp_push)
{
    std::string test_dir = fs::combine(_tmp_dir.path(), "push");
    fs::remove_directories(test_dir);
    fs::make_directories(test_dir);

    std::string user = getenv("USER");
    SshSession session;

    GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
    ASSERT_TRUE(session.connected());
    auto auth = session.auth_key_auto();
    SIHD_LOG(info, "Auth status: {}", auth.str());
    ASSERT_TRUE(auth.success());

    SshScp scp = session.make_scp();

    ASSERT_TRUE(scp.open_remote(test_dir));
    EXPECT_TRUE(scp.push_file("test/resources/file.txt", "pushed_file.txt"));
    EXPECT_TRUE(scp.push_dir("pushed_dir"));
    EXPECT_TRUE(scp.push_file("test/resources/file.txt", "pushed_file_in_dir.txt"));
    EXPECT_TRUE(scp.leave_dir());
    EXPECT_TRUE(scp.push_file("test/resources/file.txt", "pushed_file_not_in_dir.txt"));

    scp.close();
    EXPECT_FALSE(scp.push_file("test/resources/file.txt", "pushed_file2.txt"));
    EXPECT_FALSE(scp.push_dir("pushed_dir2"));
    EXPECT_FALSE(scp.leave_dir());

    EXPECT_TRUE(fs::is_file(fs::combine(test_dir, "pushed_file.txt")));
    EXPECT_TRUE(fs::is_file(fs::combine(test_dir, "pushed_dir/pushed_file_in_dir.txt")));
    EXPECT_TRUE(fs::is_file(fs::combine(test_dir, "pushed_file_not_in_dir.txt")));

    EXPECT_EQ(fs::file_size(test_dir + "/pushed_file.txt"), fs::file_size("test/resources/file.txt"));

    EXPECT_FALSE(fs::is_file(fs::combine(test_dir, "pushed_file2.txt")));
    EXPECT_FALSE(fs::is_dir(fs::combine(test_dir, "pushed_dir2")));
}

TEST_F(TestSshScp, test_sshscp_pull)
{
    std::string test_dir = fs::combine(_tmp_dir.path(), "pull");
    fs::remove_directories(test_dir);
    fs::make_directories(test_dir);

    std::string user = getenv("USER");
    SshSession session;

    GTEST_ASSERT_EQ(session.fast_connect(user, "localhost", 22), true);
    ASSERT_TRUE(session.connected());
    auto auth = session.auth_key_auto();
    SIHD_LOG(info, "Auth status: {}", auth.str());
    ASSERT_TRUE(auth.success());

    SshScp scp = session.make_scp();

    std::string pull_from = fs::combine(fs::cwd(), "test/resources/file.txt");
    std::string pull_to = fs::combine(test_dir, "pulled_file.txt");
    EXPECT_TRUE(scp.pull_file(pull_from, pull_to));
}
} // namespace test
