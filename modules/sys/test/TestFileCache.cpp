#include <filesystem>
#include <iostream>

#include <gtest/gtest.h>

#include <sihd/sys/FileCache.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/build.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::sys;
using namespace sihd::util;
class TestFileCache: public ::testing::Test
{
    protected:
        TestFileCache() { LoggerManager::stream(); }

        virtual ~TestFileCache() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!FileCache::supported)
            {
                GTEST_SKIP() << "FileCache not supported on this platform";
            }
        }

        virtual void TearDown() {}

        static std::string cached_content(FileCache & file_cache, const std::string & file_path)
        {
            const auto cached = file_cache.get(file_path);
            return cached.has_value() ? std::string(cached->get()) : std::string("<none>");
        }
};

TEST_F(TestFileCache, test_filecache_file)
{
    sihd::sys::TmpDir tmp_dir;

    std::string file_path = fs::combine({tmp_dir.path(), "test.txt"});
    std::string file_content = "hello world";
    EXPECT_TRUE(fs::write(file_path, file_content).has_value());

    FileCache file_cache;
    file_cache.add(file_path);
    auto opt_cached_value = file_cache.get(file_path);

    ASSERT_TRUE(opt_cached_value.has_value());
    EXPECT_EQ(opt_cached_value.value().get(), file_content);

    file_content = "new content";
    EXPECT_TRUE(fs::write(file_path, file_content).has_value());
    opt_cached_value = file_cache.get(file_path);
    ASSERT_TRUE(opt_cached_value.has_value());
    EXPECT_EQ(opt_cached_value.value().get(), file_content);
}

TEST_F(TestFileCache, test_filecache_unknown_or_deleted_file)
{
    TmpDir tmp_dir;
    const std::string file_path = fs::combine(tmp_dir.path(), "test.txt");
    ASSERT_TRUE(fs::write(file_path, "content").has_value());
    FileCache file_cache;
    file_cache.add(file_path);
    ASSERT_TRUE(std::filesystem::remove(file_path));

    EXPECT_EQ(cached_content(file_cache, fs::combine(tmp_dir.path(), "never_added.txt")), "<none>");
    EXPECT_EQ(cached_content(file_cache, file_path), "<none>");
}

TEST_F(TestFileCache, test_filecache_unreadable_file_is_read_once_readable)
{
    TmpDir tmp_dir;
    const std::string file_path = fs::combine(tmp_dir.path(), "test.txt");
    ASSERT_TRUE(fs::make_directory(file_path).has_value());
    FileCache file_cache;

    EXPECT_NO_THROW(file_cache.add(file_path));
    const std::string unreadable = cached_content(file_cache, file_path);
    ASSERT_TRUE(std::filesystem::remove(file_path));
    ASSERT_TRUE(fs::write(file_path, "content").has_value());
    const std::string readable = cached_content(file_cache, file_path);

    EXPECT_EQ(unreadable, "<none>");
    EXPECT_EQ(readable, "content");
}

TEST_F(TestFileCache, test_filecache_follows_atomic_replace)
{
    if constexpr (build::is_windows)
        GTEST_SKIP() << "the windows watcher hands one batch per run: the temp file batch delays the rename by one get";

    TmpDir tmp_dir;
    const std::string file_path = fs::combine(tmp_dir.path(), "test.txt");
    const std::string tmp_path = fs::combine(tmp_dir.path(), "test.txt.tmp");
    ASSERT_TRUE(fs::write(file_path, "v1").has_value());
    FileCache file_cache;
    file_cache.add(file_path);

    ASSERT_TRUE(fs::write(tmp_path, "v2").has_value());
    ASSERT_TRUE(fs::rename(tmp_path, file_path).has_value());
    const std::string replaced = cached_content(file_cache, file_path);
    ASSERT_TRUE(fs::write(file_path, "v3").has_value());
    const std::string rewritten = cached_content(file_cache, file_path);

    EXPECT_EQ(replaced, "v2");
    EXPECT_EQ(rewritten, "v3");
}

} // namespace test
