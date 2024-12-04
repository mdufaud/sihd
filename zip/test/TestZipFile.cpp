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
class TestZipFile: public ::testing::Test
{
    protected:
        TestZipFile() { sihd::util::LoggerManager::stream(); }

        virtual ~TestZipFile() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestZipFile, test_zip_write_read)
{
    const std::string replacement_string = "tis a replacement of content";
    const TmpDir tmp_dir;

    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");
    fs::remove_file(zip_path);

    ZipFile zip_writer(zip_path);

    ASSERT_TRUE(zip_writer.is_open());

    EXPECT_TRUE(zip_writer.comment_archive("tis an archive"));

    EXPECT_TRUE(zip_writer.add_dir("hello"));
    EXPECT_EQ(zip_writer.count_entries(), 1);

    EXPECT_TRUE(zip_writer.add_file("hello/world", "Hello world !"));
    EXPECT_FALSE(zip_writer.add_file("hello/world", "Should not be written"));

    EXPECT_TRUE(zip_writer.load_entry("hello/world"));
    EXPECT_TRUE(zip_writer.rename_entry("hello/renamed"));
    EXPECT_TRUE(zip_writer.modify_entry_time(Calendar {.day = 21, .month = 2, .year = 2000}));
    EXPECT_TRUE(zip_writer.comment_entry("Entry commentary"));

    EXPECT_TRUE(zip_writer.load_entry("hello/"));
    EXPECT_TRUE(zip_writer.is_entry_directory());
    EXPECT_EQ(zip_writer.count_entries(), 2);

    EXPECT_TRUE(zip_writer.add_file("hello/world", "Should be replaced !"));
    EXPECT_TRUE(zip_writer.load_entry("hello/world"));
    EXPECT_TRUE(zip_writer.replace_entry(replacement_string));
    EXPECT_EQ(zip_writer.count_entries(), 3);

    EXPECT_TRUE(zip_writer.add_dir("to"));
    EXPECT_TRUE(zip_writer.add_file("to/remove", ""));
    EXPECT_EQ(zip_writer.count_entries(), 5);
    EXPECT_TRUE(zip_writer.load_entry("to/"));
    EXPECT_TRUE(zip_writer.remove_entry());
    EXPECT_TRUE(zip_writer.load_entry("to/remove"));
    EXPECT_TRUE(zip_writer.remove_entry());
    EXPECT_EQ(zip_writer.count_entries(), 5);

    EXPECT_EQ(zip_writer.count_original_entries(), 0);

    ASSERT_TRUE(zip_writer.close());

    ZipFile zip_reader(zip_path);

    ASSERT_TRUE(zip_reader.is_open());

    EXPECT_TRUE(zip_reader.load_entry("hello/"));
    EXPECT_TRUE(zip_reader.is_entry_loaded());
    EXPECT_TRUE(zip_reader.is_entry_directory());

    ArrCharView view;

    EXPECT_TRUE(zip_reader.load_entry("hello/world"));
    EXPECT_FALSE(zip_reader.is_entry_directory());
    zip_reader.set_buffer_size(200);
    EXPECT_EQ(zip_reader.read_entry(), (ssize_t)replacement_string.size());
    EXPECT_TRUE(zip_reader.get_read_data(view));
    EXPECT_EQ(view.str(), replacement_string);
    EXPECT_EQ(zip_reader.read_entry(), 0);
}

} // namespace test
