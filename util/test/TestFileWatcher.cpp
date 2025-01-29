#include <filesystem>
#include <fstream>
#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/util/FileWatcher.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::util;
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
    sihd::util::TmpDir tmp_dir;

    std::filesystem::path test_path(tmp_dir.path());
    test_path = test_path / "test";
    std::filesystem::create_directories(test_path);

    std::filesystem::path test_file = test_path / "file.txt";

    FileWatcher fw(test_path.string(), [](FileWatcherEvent) {});

    std::ofstream file(test_file);
    file << "hello\n";
    file.close();

    fw.poll();
}

} // namespace test
