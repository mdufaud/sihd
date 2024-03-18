#include <gtest/gtest.h>

#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>

#include <sihd/zip/ZipFile.hpp>

#include <sihd/util/Process.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::zip;
using namespace sihd::util;
class TestZip: public ::testing::Test
{
    protected:
        TestZip() { sihd::util::LoggerManager::basic(); }

        virtual ~TestZip() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestZip, test_zip)
{
    TmpDir tmp_dir;

    std::string zip_path = fs::combine(tmp_dir.path(), "to_zip.zip");
    fs::remove_file(zip_path);

    SIHD_LOG_INFO("{}", zip_path);

    ZipFile zip(zip_path);

    ASSERT_TRUE(zip.is_open());

    EXPECT_TRUE(zip.comment_archive("tis an archive"));

    EXPECT_TRUE(zip.add_dir("hello"));
    EXPECT_EQ(zip.count_entries(), 1);

    EXPECT_TRUE(zip.add_file("hello/world", "hello world !"));
    EXPECT_FALSE(zip.add_file("hello/world", "nope"));

    EXPECT_TRUE(zip.load_entry("hello/world"));
    EXPECT_TRUE(zip.rename_entry("hello/renamed"));
    EXPECT_TRUE(zip.modify_entry_time(Calendar {.day = 21, .month = 2, .year = 2000}));
    EXPECT_TRUE(zip.comment_entry("tis an entry"));

    EXPECT_TRUE(zip.load_entry("hello/"));
    EXPECT_TRUE(zip.is_entry_directory());
    EXPECT_EQ(zip.count_entries(), 2);

    EXPECT_TRUE(zip.add_file("hello/world", "hi !"));
    EXPECT_TRUE(zip.load_entry("hello/world"));
    EXPECT_TRUE(zip.replace_entry("tis a replacement of content"));
    EXPECT_EQ(zip.count_entries(), 3);

    EXPECT_TRUE(zip.add_dir("to"));
    EXPECT_TRUE(zip.add_file("to/remove", ""));
    EXPECT_EQ(zip.count_entries(), 5);
    EXPECT_TRUE(zip.load_entry("to/"));
    EXPECT_TRUE(zip.remove_entry());
    EXPECT_TRUE(zip.load_entry("to/remove"));
    EXPECT_TRUE(zip.remove_entry());
    EXPECT_EQ(zip.count_entries(), 5);

    EXPECT_EQ(zip.count_original_entries(), 0);

    ASSERT_TRUE(zip.close());

    Process("unzip", "-vl", zip_path).run();
    Process("ls").run();
    Process("ls", "-la").run();
}

} // namespace test
