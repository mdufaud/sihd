#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/path.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
class TestPath: public ::testing::Test
{
    protected:
        TestPath() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPath() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() { path::clear_all(); }

        virtual void TearDown() {}
};

TEST_F(TestPath, test_path_get)
{
    TmpDir tmp_dir;

    std::filesystem::path test_path(tmp_dir.path());
    test_path.append("test_path");
    std::filesystem::path test_dir = test_path / "1" / "2";
    std::filesystem::create_directories(test_dir);
    auto test_file = test_dir / "file.txt";
    std::ofstream file(test_file.string());
    file << "hello" << std::endl;
    file.close();

    path::set("", "");
    path::set("false-dir", "/does/not/exists");
    path::set("sihd-test", test_dir.string());
    EXPECT_EQ(path::get("sihd-test", "file.txt"), test_dir / "file.txt");
    EXPECT_EQ(path::get("sihd-test://file.txt"), test_dir / "file.txt");
    EXPECT_EQ(path::find("file.txt"), test_dir / "file.txt");

    path::clear("sihd-test");
    EXPECT_EQ(path::get("sihd-test", "file.txt"), "");

    SIHD_TRACEF(path::find("test/TestPath.cpp"));
}
} // namespace test
