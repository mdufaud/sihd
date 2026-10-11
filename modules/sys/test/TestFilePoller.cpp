#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <sihd/sys/FilePoller.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

namespace
{

// recursive_children reports directories with a trailing separator on posix and none on windows
std::filesystem::path normalized_path(std::string path)
{
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();
    return std::filesystem::path(std::move(path));
}

} // namespace

class TestFilePoller: public ::testing::Test
{
    protected:
        TestFilePoller() { sihd::util::LoggerManager::stream(); }

        virtual ~TestFilePoller() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestFilePoller, test_filepoller_created_removed_changed)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string watched = tmp_dir.path() + "/watched";
    ASSERT_TRUE(std::filesystem::create_directory(watched));

    FilePoller poller;
    ASSERT_TRUE(poller.watch(watched, 1).has_value());
    EXPECT_EQ(poller.watch_path(), watched);
    EXPECT_EQ(poller.watch_size(), 1u);

    EXPECT_TRUE(poller.run());

    const std::string file_a = watched + "/a.txt";
    {
        std::ofstream f(file_a);
        f << "one";
    }
    EXPECT_TRUE(poller.run());
    ASSERT_EQ(poller.created().size(), 1u);
    EXPECT_EQ(normalized_path(poller.created()[0]), normalized_path(file_a));
    EXPECT_TRUE(poller.removed().empty());

    time::msleep(20);
    {
        std::ofstream f(file_a, std::ios::app);
        f << "two";
    }
    EXPECT_TRUE(poller.run());
    ASSERT_EQ(poller.changed().size(), 1u);
    EXPECT_EQ(normalized_path(poller.changed()[0]), normalized_path(file_a));
    EXPECT_TRUE(poller.created().empty());

    const std::string nested = watched + "/nested";
    ASSERT_TRUE(std::filesystem::create_directory(nested));
    EXPECT_TRUE(poller.run());
    ASSERT_EQ(poller.created().size(), 1u);
    EXPECT_EQ(normalized_path(poller.created()[0]), normalized_path(nested));

    {
        std::ofstream f(nested + "/deep.txt");
        f << "deep";
    }
    EXPECT_TRUE(poller.run());
    EXPECT_TRUE(poller.created().empty());

    std::filesystem::remove(file_a);
    EXPECT_TRUE(poller.run());
    ASSERT_EQ(poller.removed().size(), 1u);
    EXPECT_EQ(normalized_path(poller.removed()[0]), normalized_path(file_a));
}

TEST_F(TestFilePoller, test_filepoller_unwatch_stops_reporting)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string watched = tmp_dir.path() + "/watched";
    ASSERT_TRUE(std::filesystem::create_directory(watched));

    FilePoller poller(watched, 1);
    EXPECT_TRUE(poller.run());

    poller.unwatch();
    EXPECT_TRUE(poller.watch_path().empty());
    EXPECT_FALSE(poller.run());

    ASSERT_TRUE(poller.watch(watched, 1).has_value());
    EXPECT_TRUE(poller.run());

    const std::string file_b = watched + "/b.txt";
    {
        std::ofstream f(file_b);
        f << "reborn";
    }
    EXPECT_TRUE(poller.run());
    ASSERT_EQ(poller.created().size(), 1u);
    EXPECT_EQ(normalized_path(poller.created()[0]), normalized_path(file_b));
}

} // namespace test
