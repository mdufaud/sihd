#include <filesystem>
#include <fstream>
#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/sys/FileWatcher.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
class TestFileWatcher: public ::testing::Test
{
    protected:
        TestFileWatcher() { sihd::util::LoggerManager::stream(); }

        virtual ~TestFileWatcher() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestFileWatcher, test_filewatcher_file)
{
    sihd::sys::TmpDir tmp_dir;

    std::filesystem::path test_path(tmp_dir.path());
    test_path = test_path / "test";
    std::filesystem::create_directories(test_path);

    std::filesystem::path test_file = test_path / "file.txt";

    FileWatcher fw(test_path.string());

    std::ofstream file(test_file);
    file << "hello\n";
    file.close();

    fw.run();
}

} // namespace test
