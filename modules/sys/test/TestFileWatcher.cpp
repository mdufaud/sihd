#include <filesystem>
#include <fstream>
#include <iostream>

#include <gtest/gtest.h>

#include <sihd/sys/FileWatcher.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/term.hpp>

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

        virtual void SetUp()
        {
            if constexpr (!FileWatcher::supported)
            {
                GTEST_SKIP() << "filesystem-watch backend not supported on this platform";
            }
        }

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

TEST_F(TestFileWatcher, test_filewatcher_events)
{
    sihd::sys::TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);

    std::filesystem::path test_path(tmp_dir.path());
    test_path = test_path / "watch";
    std::filesystem::create_directories(test_path);

    FileWatcher fw(test_path.string());
    EXPECT_TRUE(fw.is_watching(test_path.string()));

    const std::filesystem::path test_file = test_path / "file.txt";
    {
        std::ofstream file(test_file);
        file << "hello\n";
    }
    ASSERT_TRUE(fw.run());
    bool created = false;
    for (const auto & ev : fw.events())
        created = created || (ev.type == FileWatcherEventType::created && ev.filename == "file.txt");
    EXPECT_TRUE(created);

    {
        std::ofstream file(test_file, std::ios::app);
        file << "more\n";
    }
    ASSERT_TRUE(fw.run());
    bool modified = false;
    for (const auto & ev : fw.events())
        modified = modified || (ev.type == FileWatcherEventType::modified && ev.filename == "file.txt");
    EXPECT_TRUE(modified);

    std::filesystem::remove(test_file);
    ASSERT_TRUE(fw.run());
#if !defined(__SIHD_WINDOWS__)
    bool deleted = false;
    for (const auto & ev : fw.events())
        deleted = deleted || (ev.type == FileWatcherEventType::deleted && ev.filename == "file.txt");
    EXPECT_TRUE(deleted);
#endif

    ASSERT_TRUE(fw.unwatch(test_path.string()));
    EXPECT_FALSE(fw.is_watching(test_path.string()));
    fw.clear();
    EXPECT_FALSE(fw.is_watching(test_path.string()));
}

} // namespace test
