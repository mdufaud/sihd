#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/PathManager.hpp>
#include <sihd/util/TmpDir.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestPathManager: public ::testing::Test
{
    protected:
        TestPathManager() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPathManager() { sihd::util::LoggerManager::clear_loggers(); }

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

    // Test push/remove/find/clear

    EXPECT_TRUE(path_manager.find("parent").empty());
    EXPECT_TRUE(path_manager.find("random-dir").empty());
    path_manager.push_back(test_path.string());
    EXPECT_FALSE(path_manager.find("parent").empty());
    EXPECT_FALSE(path_manager.find("random-dir").empty());

    EXPECT_TRUE(path_manager.find("child").empty());
    path_manager.push_back(test_path_parent.string());
    EXPECT_FALSE(path_manager.find("child").empty());

    path_manager.remove(test_path.string());
    EXPECT_TRUE(path_manager.find("parent").empty());
    EXPECT_TRUE(path_manager.find("random-dir").empty());
    EXPECT_FALSE(path_manager.find("child").empty());

    path_manager.clear();
    EXPECT_TRUE(path_manager.find("child").empty());

    // Test file

    path_manager.push_back(test_path_deep.string());

    EXPECT_EQ(path_manager.find("file.txt"), test_file_path.string());

    std::filesystem::permissions(test_file_path,
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);

    path_manager.clear();

    // Test find order and findall with twin

    path_manager.push_back(test_path.string());
    path_manager.push_back(test_path_parent.string());
    EXPECT_EQ(path_manager.find("twin"), test_path_twin.string());
    path_manager.remove(test_path_parent.string());
    path_manager.push_front(test_path_parent.string());
    EXPECT_EQ(path_manager.find("twin"), test_path_parent_twin.string());

    std::vector<std::string> all_twins = path_manager.find_all("twin");
    ASSERT_EQ(all_twins.size(), 2);
    EXPECT_EQ(all_twins[0], test_path_parent_twin.string());
    EXPECT_EQ(all_twins[1], test_path_twin.string());
}
} // namespace test
