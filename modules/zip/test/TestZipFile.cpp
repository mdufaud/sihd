#include <gtest/gtest.h>

#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Array.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>
#include <sihd/zip/ZipFile.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::zip;
using namespace sihd::util;
using namespace sihd::sys;
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
    (void)fs::remove_file(zip_path);

    ZipFile zip_writer(zip_path);

    ASSERT_TRUE(zip_writer.is_open());

    EXPECT_TRUE(zip_writer.comment_archive("tis an archive").has_value());

    EXPECT_TRUE(zip_writer.add_dir("hello").has_value());
    EXPECT_EQ(zip_writer.count_entries(), 1);

    EXPECT_TRUE(zip_writer.add_file("hello/world", "Hello world !").has_value());
    EXPECT_FALSE(zip_writer.add_file("hello/world", "Should not be written").has_value());

    EXPECT_TRUE(zip_writer.load_entry("hello/world").has_value());
    EXPECT_TRUE(zip_writer.rename_entry("hello/renamed").has_value());
    EXPECT_TRUE(zip_writer.modify_entry_time(Calendar {.day = 21, .month = 2, .year = 2000}).has_value());
    EXPECT_TRUE(zip_writer.comment_entry("Entry commentary").has_value());

    EXPECT_TRUE(zip_writer.load_entry("hello/").has_value());
    EXPECT_TRUE(zip_writer.is_entry_directory());
    EXPECT_EQ(zip_writer.count_entries(), 2);

    EXPECT_TRUE(zip_writer.add_file("hello/world", "Should be replaced !").has_value());
    EXPECT_TRUE(zip_writer.load_entry("hello/world").has_value());
    EXPECT_TRUE(zip_writer.replace_entry(replacement_string).has_value());
    EXPECT_EQ(zip_writer.count_entries(), 3);

    EXPECT_TRUE(zip_writer.add_dir("to").has_value());
    EXPECT_TRUE(zip_writer.add_file("to/remove", "").has_value());
    EXPECT_EQ(zip_writer.count_entries(), 5);
    EXPECT_TRUE(zip_writer.load_entry("to/").has_value());
    EXPECT_TRUE(zip_writer.remove_entry().has_value());
    EXPECT_TRUE(zip_writer.load_entry("to/remove").has_value());
    EXPECT_TRUE(zip_writer.remove_entry().has_value());
    EXPECT_EQ(zip_writer.count_entries(), 5);

    EXPECT_EQ(zip_writer.count_original_entries(), 0);

    ASSERT_TRUE(zip_writer.close().has_value());

    ZipFile zip_reader(zip_path);

    ASSERT_TRUE(zip_reader.is_open());

    EXPECT_TRUE(zip_reader.load_entry("hello/").has_value());
    EXPECT_TRUE(zip_reader.is_entry_loaded());
    EXPECT_TRUE(zip_reader.is_entry_directory());

    ArrCharView view;

    EXPECT_TRUE(zip_reader.load_entry("hello/world").has_value());
    EXPECT_FALSE(zip_reader.is_entry_directory());
    (void)zip_reader.set_buffer_size(200);
    auto replacement_read = zip_reader.read_entry();
    ASSERT_TRUE(replacement_read.has_value());
    EXPECT_EQ(replacement_read.value(), (ssize_t)replacement_string.size());
    EXPECT_TRUE(zip_reader.get_read_data(view));
    EXPECT_EQ(view.str(), replacement_string);
    auto end_read = zip_reader.read_entry();
    ASSERT_TRUE(end_read.has_value());
    EXPECT_EQ(end_read.value(), 0);
}

TEST_F(TestZipFile, test_zip_not_open)
{
    ZipFile zip;

    EXPECT_FALSE(zip.is_open());
    EXPECT_EQ(zip.count_entries(), -1);
    EXPECT_EQ(zip.count_original_entries(), -1);
    EXPECT_FALSE(zip.load_entry(0).has_value());
    EXPECT_FALSE(zip.load_entry("nope").has_value());
    EXPECT_FALSE(zip.is_entry_loaded());
    EXPECT_FALSE(zip.is_entry_directory());
    auto next = zip.read_next_entry();
    ASSERT_FALSE(next.has_value());
    EXPECT_FALSE(zip.read_next().has_value());

    ArrCharView view;
    EXPECT_FALSE(zip.get_read_data(view));

    EXPECT_FALSE(zip.add_dir("dir").has_value());
    EXPECT_FALSE(zip.add_file("file", "data").has_value());
    EXPECT_FALSE(zip.add_from_fs("name", "/nonexistent").has_value());
    EXPECT_FALSE(zip.add_file_from_fs("name", "/nonexistent").has_value());
    EXPECT_FALSE(zip.add_dir_from_fs("name", "/nonexistent").has_value());
    EXPECT_FALSE(zip.dump_entry_to_fs("/tmp/nope").has_value());

    EXPECT_FALSE(zip.set_buffer_size(0).has_value());
    EXPECT_FALSE(zip.set_password("pass").has_value());
    // close on a not-open zip is a no-op and returns true
    EXPECT_TRUE(zip.close().has_value());
}

TEST_F(TestZipFile, test_zip_entry_not_loaded)
{
    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");

    ZipFile zip(zip_path);
    ASSERT_TRUE(zip.is_open());

    EXPECT_FALSE(zip.is_entry_loaded());
    EXPECT_FALSE(zip.unchange_entry().has_value());
    EXPECT_FALSE(zip.remove_entry().has_value());
    EXPECT_FALSE(zip.rename_entry("whatever").has_value());
    EXPECT_FALSE(zip.modify_entry_time(sihd::util::Timestamp(0)).has_value());
    EXPECT_FALSE(zip.comment_entry("comment").has_value());
    EXPECT_FALSE(zip.encrypt_entry("pass").has_value());
    EXPECT_FALSE(zip.replace_entry("data").has_value());
    EXPECT_FALSE(zip.read_entry().has_value());
}

TEST_F(TestZipFile, test_zip_error_codes)
{
    const TmpDir tmp_dir;
    const std::string not_zip_path = fs::combine(tmp_dir.path(), "not_a_zip.zip");
    ASSERT_TRUE(fs::write(not_zip_path, "this is not a zip archive").has_value());

    ZipFile zip;
    auto opened = zip.open(not_zip_path, true);
    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, not_found);
    EXPECT_FALSE(opened.error().message.empty());

    auto loaded = zip.load_entry(0);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().code, not_initialized);
    EXPECT_FALSE(loaded.error().message.empty());

    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");

    ZipFile writer(zip_path);
    ASSERT_TRUE(writer.is_open());
    EXPECT_TRUE(writer.add_file("file.txt", "data").has_value());
    auto duplicate = writer.add_file("file.txt", "other data");
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error().code, already_exists);

    EXPECT_TRUE(writer.close().has_value());

    ZipFile reader(zip_path, true);
    ASSERT_TRUE(reader.is_open());
    EXPECT_TRUE(reader.load_entry("file.txt").has_value());
    EXPECT_EQ(reader.current_entry().size, 4u);
}

TEST_F(TestZipFile, test_zip_add_file_from_fs)
{
    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");
    const std::string file_path = fs::combine(tmp_dir.path(), "file.txt");
    const std::string content = "hello from filesystem";

    ASSERT_TRUE(fs::write(file_path, content).has_value());

    {
        ZipFile zip(zip_path);
        ASSERT_TRUE(zip.is_open());
        // this is the first entry (index 0) - previously returned false due to ssize_t -> bool bug
        EXPECT_TRUE(zip.add_file_from_fs("first_file.txt", file_path).has_value());
        EXPECT_TRUE(zip.add_file_from_fs("second_file.txt", file_path).has_value());
        ASSERT_TRUE(zip.close().has_value());
    }

    {
        ZipFile zip(zip_path, true);
        ASSERT_TRUE(zip.is_open());
        EXPECT_EQ(zip.count_entries(), 2);

        EXPECT_TRUE(zip.load_entry("first_file.txt").has_value());
        EXPECT_FALSE(zip.is_entry_directory());
        EXPECT_EQ(zip.current_entry().size, content.size());

        (void)zip.set_buffer_size(256);
        auto content_read = zip.read_entry();
        ASSERT_TRUE(content_read.has_value());
        EXPECT_EQ(content_read.value(), (ssize_t)content.size());

        ArrCharView view;
        EXPECT_TRUE(zip.get_read_data(view));
        EXPECT_EQ(view.str(), content);
    }
}

TEST_F(TestZipFile, test_zip_multi_chunk_read)
{
    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");
    const std::string data = str::generate_random(10000);

    {
        ZipFile zip(zip_path);
        ASSERT_TRUE(zip.is_open());
        EXPECT_TRUE(zip.add_file("big_file.txt", data).has_value());
        ASSERT_TRUE(zip.close().has_value());
    }

    {
        ZipFile zip(zip_path, true);
        ASSERT_TRUE(zip.is_open());
        EXPECT_TRUE(zip.load_entry("big_file.txt").has_value());

        // small buffer forces multiple reads
        (void)zip.set_buffer_size(128);

        std::string result;
        size_t bytes_read = 1;
        while (true)
        {
            auto read = zip.read_entry();
            ASSERT_TRUE(read.has_value());
            if (read.value() == 0)
            {
                bytes_read = 0;
                break;
            }
            bytes_read = (size_t)read.value();
            ArrCharView view;
            ASSERT_TRUE(zip.get_read_data(view));
            result.append(view.data(), view.size());
        }
        EXPECT_EQ(bytes_read, 0u);
        EXPECT_EQ(result, data);
    }
}

TEST_F(TestZipFile, test_zip_ireader_interface)
{
    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");
    const std::string content = "IReader test content";

    {
        ZipFile zip(zip_path);
        ASSERT_TRUE(zip.is_open());
        EXPECT_TRUE(zip.add_file("reader_test.txt", content).has_value());
        ASSERT_TRUE(zip.close().has_value());
    }

    {
        ZipFile zip(zip_path, true);
        ASSERT_TRUE(zip.is_open());
        EXPECT_TRUE(zip.load_entry("reader_test.txt").has_value());

        (void)zip.set_buffer_size(256);

        IReader & reader = zip;
        auto first = reader.read_next();
        ASSERT_TRUE(first.has_value());
        EXPECT_TRUE(first.value());

        ArrCharView view;
        EXPECT_TRUE(reader.get_read_data(view));
        EXPECT_EQ(view.str(), content);

        // second read_next should return false (no more data)
        auto second = reader.read_next();
        ASSERT_TRUE(second.has_value());
        EXPECT_FALSE(second.value());
    }
}

TEST_F(TestZipFile, test_zip_read_existing_archive)
{
    ZipFile zip("test/resources/to_read/to_zip.zip", true, true);

    ASSERT_TRUE(zip.is_open());

    const ssize_t entry_count = zip.count_entries();
    EXPECT_GT(entry_count, 0);

    // iterate all entries
    size_t dir_count = 0;
    size_t file_count = 0;
    for (;;)
    {
        auto next = zip.read_next_entry();
        ASSERT_TRUE(next.has_value()) << next.error().message;
        if (!next.value())
            break;

        EXPECT_TRUE(zip.is_entry_loaded());
        EXPECT_GT(zip.entry_name().size(), 0u);

        if (zip.is_entry_directory())
            ++dir_count;
        else
            ++file_count;
    }
    EXPECT_EQ((ssize_t)(dir_count + file_count), entry_count);
    EXPECT_GT(file_count, 0u);
}

TEST_F(TestZipFile, test_zip_discard)
{
    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");

    {
        ZipFile zip(zip_path);
        ASSERT_TRUE(zip.is_open());
        EXPECT_TRUE(zip.add_file("file.txt", "some content").has_value());
        zip.discard();
        EXPECT_FALSE(zip.is_open());
    }

    // archive should not exist or be empty since we discarded
    EXPECT_FALSE(fs::exists(zip_path));
}

TEST_F(TestZipFile, test_zip_archive_comment_roundtrip)
{
    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");
    const std::string comment = "This is an archive comment";

    {
        ZipFile zip(zip_path);
        ASSERT_TRUE(zip.is_open());
        EXPECT_TRUE(zip.comment_archive(comment).has_value());
        EXPECT_TRUE(zip.add_dir("empty_dir").has_value());
        ASSERT_TRUE(zip.close().has_value());
    }

    {
        ZipFile zip(zip_path, true);
        ASSERT_TRUE(zip.is_open());
        EXPECT_EQ(zip.archive_comment(), comment);
    }
}

TEST_F(TestZipFile, test_zip_entry_comment_roundtrip)
{
    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");
    const std::string entry_comment = "Entry comment text";

    {
        ZipFile zip(zip_path);
        ASSERT_TRUE(zip.is_open());
        EXPECT_TRUE(zip.add_file("commented.txt", "data").has_value());
        EXPECT_TRUE(zip.load_entry("commented.txt").has_value());
        EXPECT_TRUE(zip.comment_entry(entry_comment).has_value());
        ASSERT_TRUE(zip.close().has_value());
    }

    {
        ZipFile zip(zip_path, true);
        ASSERT_TRUE(zip.is_open());
        EXPECT_TRUE(zip.load_entry("commented.txt").has_value());
        EXPECT_EQ(zip.entry_comment(), entry_comment);
    }
}

TEST_F(TestZipFile, test_zip_dump_entry_to_fs)
{
    const TmpDir tmp_dir;
    const std::string zip_path = fs::combine(tmp_dir.path(), "test.zip");
    const std::string content = "file content for dump test";

    {
        ZipFile zip(zip_path);
        ASSERT_TRUE(zip.is_open());
        EXPECT_TRUE(zip.add_dir("subdir/").has_value());
        EXPECT_TRUE(zip.add_file("subdir/file.txt", content).has_value());
        ASSERT_TRUE(zip.close().has_value());
    }

    {
        ZipFile zip(zip_path, true);
        ASSERT_TRUE(zip.is_open());

        // dump directory
        EXPECT_TRUE(zip.load_entry("subdir/").has_value());
        const std::string dir_out = fs::combine(tmp_dir.path(), "out_dir");
        EXPECT_TRUE(zip.dump_entry_to_fs(dir_out).has_value());
        EXPECT_TRUE(fs::is_dir(dir_out));

        // dump file
        EXPECT_TRUE(zip.load_entry("subdir/file.txt").has_value());
        const std::string file_out = fs::combine(dir_out, "file.txt");
        EXPECT_TRUE(zip.dump_entry_to_fs(file_out).has_value());
        EXPECT_TRUE(fs::is_file(file_out));

        const auto read_content = fs::read_all(file_out);
        ASSERT_TRUE(read_content.has_value());
        EXPECT_EQ(read_content.value(), content);
    }
}

} // namespace test
