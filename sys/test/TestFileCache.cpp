#include <iostream>

#include <gtest/gtest.h>

#include <sihd/sys/FileCache.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::sys;
using namespace sihd::util;
class TestFileCache: public ::testing::Test
{
    protected:
        TestFileCache() { sihd::util::LoggerManager::stream(); }

        virtual ~TestFileCache() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestFileCache, test_filecache_file)
{
    sihd::sys::TmpDir tmp_dir;

    std::string file_path = fs::combine({tmp_dir.path(), "test.txt"});
    std::string file_content = "hello world";
    EXPECT_TRUE(fs::write(file_path, file_content));

    FileCache file_cache;
    file_cache.add(file_path);
    auto opt_cached_value = file_cache.get(file_path);

    ASSERT_TRUE(opt_cached_value.has_value());
    EXPECT_EQ(opt_cached_value.value().get(), file_content);

    file_content = "new content";
    EXPECT_TRUE(fs::write(file_path, file_content));
    opt_cached_value = file_cache.get(file_path);
    ASSERT_TRUE(opt_cached_value.has_value());
    EXPECT_EQ(opt_cached_value.value().get(), file_content);
}

} // namespace test
