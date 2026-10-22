#include <filesystem>
#include <fstream>
#include <iostream>

#include <gtest/gtest.h>

#include <sihd/sys/FileWatcher.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;
class TestFileWatcher: public ::testing::Test
{
    protected:
        TestFileWatcher() { LoggerManager::stream(); }

        virtual ~TestFileWatcher() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!FileWatcher::supported)
            {
                GTEST_SKIP() << "filesystem-watch backend not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestFileWatcher, test_watch_renamed_file)
{
    if (!FileWatcher::supported)
        GTEST_SKIP();
    TmpDir directory;
    ASSERT_TRUE(directory);
    const std::string path = fs::combine(directory.path(), "file-\xc3\xa9.txt");
    const std::string renamed = fs::combine(directory.path(), "renamed.txt");
    const std::filesystem::path native_path(reinterpret_cast<const char8_t *>(path.c_str()));
    const std::filesystem::path native_renamed(reinterpret_cast<const char8_t *>(renamed.c_str()));
    {
        std::ofstream file(native_path);

        ASSERT_TRUE(file);
        file << "data";
    }
    FileWatcher watcher(path, 1000);
    std::filesystem::rename(native_path, native_renamed);
    EXPECT_TRUE(watcher.run());
    EXPECT_FALSE(watcher.events().empty());
    if constexpr (build::is_windows)
    {
        const auto event = std::ranges::find_if(watcher.events(), [](const FileWatcherEvent & event) {
            return event.type == FileWatcherEventType::renamed;
        });
        ASSERT_NE(event, watcher.events().end());
        EXPECT_EQ(event->old_filename, "file-\xc3\xa9.txt");
        EXPECT_EQ(event->filename, "renamed.txt");
    }
}

TEST_F(TestFileWatcher, test_watch_separate_buffers)
{
    if (!FileWatcher::supported)
        GTEST_SKIP();
    TmpDir first;
    TmpDir second;
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    FileWatcher watcher;

    watcher.set_run_timeout(1000);
    ASSERT_TRUE(watcher.watch(first.path()));
    ASSERT_TRUE(watcher.watch(second.path()));
    ASSERT_TRUE(fs::write(fs::combine(first.path(), "first.txt"), "first"));
    ASSERT_TRUE(fs::write(fs::combine(second.path(), "second.txt"), "second"));
    EXPECT_TRUE(watcher.run());
    bool found_first = false;
    bool found_second = false;

    for (const FileWatcherEvent & event : watcher.events())
    {
        if (event.type != FileWatcherEventType::created)
            continue;
        if (event.watch_path == first.path())
        {
            EXPECT_EQ(event.filename, "first.txt");
            found_first = true;
        }
        if (event.watch_path == second.path())
        {
            EXPECT_EQ(event.filename, "second.txt");
            found_second = true;
        }
    }
    EXPECT_TRUE(found_first);
    EXPECT_TRUE(found_second);
}

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
