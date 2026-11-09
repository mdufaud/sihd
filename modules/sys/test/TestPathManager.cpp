#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/sys/PathManager.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
class TestPathManager: public ::testing::Test
{
    protected:
        TestPathManager() { LoggerManager::stream(); }

        virtual ~TestPathManager() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPathManager, test_pathmanager_get)
{
    TmpDir tmp_dir;

    /**
     * /tmp/xxx/test_path
     * /tmp/xxx/test_path/random-dir
     * /tmp/xxx/test_path/twin
     * /tmp/xxx/test_path/parent
     * /tmp/xxx/test_path/parent/twin
     * /tmp/xxx/test_path/parent/child
     * /tmp/xxx/test_path/parent/child/file.txt
     */

    std::filesystem::path test_path(tmp_dir.path());
    test_path.append("test_path");
    std::filesystem::create_directories(test_path / "random-dir");
    std::filesystem::path test_path_twin = test_path / "twin";
    std::filesystem::create_directories(test_path_twin);

    std::filesystem::path test_path_parent = test_path / "parent";
    std::filesystem::path test_path_deep = test_path_parent / "child";
    std::filesystem::path test_path_parent_twin = test_path_parent / "twin";

    std::filesystem::create_directories(test_path_parent_twin);
    std::filesystem::create_directories(test_path_deep);

    auto test_file_path = test_path_deep / "file.txt";
    std::ofstream file(test_file_path);
    file << "hello" << std::endl;
    file.close();

    PathManager path_manager;

    EXPECT_TRUE(path_manager.find("parent").empty());
    EXPECT_TRUE(path_manager.find("random-dir").empty());
    path_manager.push_back(test_path.string());
    EXPECT_FALSE(path_manager.find("parent").empty());
    EXPECT_FALSE(path_manager.find("random-dir").empty());

    EXPECT_TRUE(path_manager.find("child").empty());
    path_manager.push_back(test_path_parent.string());
    EXPECT_FALSE(path_manager.find("child").empty());

    (void)path_manager.remove(test_path.string());
    EXPECT_TRUE(path_manager.find("parent").empty());
    EXPECT_TRUE(path_manager.find("random-dir").empty());
    EXPECT_FALSE(path_manager.find("child").empty());

    path_manager.clear();
    EXPECT_TRUE(path_manager.find("child").empty());

    path_manager.push_back(test_path_deep.string());

    EXPECT_EQ(path_manager.find("file.txt"), test_file_path.string());

    std::filesystem::permissions(test_file_path,
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);

    path_manager.clear();

    path_manager.push_back(test_path.string());
    path_manager.push_back(test_path_parent.string());
    EXPECT_EQ(path_manager.find("twin"), test_path_twin.string());
    (void)path_manager.remove(test_path_parent.string());
    path_manager.push_front(test_path_parent.string());
    EXPECT_EQ(path_manager.find("twin"), test_path_parent_twin.string());

    std::vector<std::string> all_twins = path_manager.find_all("twin");
    ASSERT_EQ(all_twins.size(), 2U);
    EXPECT_EQ(all_twins[0], test_path_parent_twin.string());
    EXPECT_EQ(all_twins[1], test_path_twin.string());
}

TEST_F(TestPathManager, test_pathmanager_concurrent_push_and_find)
{
    PathManager path_manager;

    std::thread pusher([&] {
        for (int i = 0; i < 200; ++i)
            path_manager.push_back(fmt::format("/sihd_nonexistent_{}", i));
    });
    for (int i = 0; i < 200; ++i)
        EXPECT_EQ(path_manager.find("file.txt"), "");
    pusher.join();

    EXPECT_EQ(path_manager.path_lst().size(), 200U);
}

TEST_F(TestPathManager, test_pathmanager_find_skips_unsearchable_directory)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "windows has no directory search permission";
    if (user::is_root())
        GTEST_SKIP() << "root searches regardless of permissions";

    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string locked = fs::combine(tmp_dir.path(), "locked");
    const std::string file = fs::combine(tmp_dir.path(), "file.txt");
    ASSERT_TRUE(std::filesystem::create_directory(locked));
    ASSERT_TRUE(fs::write(file, "content").has_value());
    ASSERT_TRUE(fs::permission_set(locked, 0).has_value());
    Defer restore([&] { (void)fs::permission_set(locked, 0700); });
    PathManager path_manager;
    path_manager.push_back(locked);
    path_manager.push_back(tmp_dir.path());

    EXPECT_EQ(path_manager.find("file.txt"), file);
    EXPECT_EQ(path_manager.find_all("file.txt"), std::vector<std::string> {file});
}
} // namespace test
