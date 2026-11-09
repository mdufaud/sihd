#include <cstring>

#include <gtest/gtest.h>

#include <sihd/sys/File.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/build.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

class TestFile: public ::testing::Test
{
    protected:
        TestFile() = default;
        virtual ~TestFile() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}

        static std::string
            content_after_write(const std::string & path, const File::OpenOptions & options, std::string_view data)
        {
            File file;
            if (!file.open(path, options).has_value() || !file.write(data.data(), data.size()).has_value()
                || !file.close().has_value())
                return "<failed>";
            return fs::read_all(path).value_or("<unreadable>");
        }
};

TEST_F(TestFile, test_file_write_read)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "test.txt");
    {
        File file(path, "w");
        ASSERT_TRUE(file.is_open());
        auto wrote = file.write("hello", 5);
        ASSERT_TRUE(wrote.has_value());
        EXPECT_EQ(wrote.value(), 5u);
    }

    {
        File file(path, "r");
        ASSERT_TRUE(file.is_open());
        char buf[16] = {};
        auto read = file.read(buf, sizeof(buf));
        ASSERT_TRUE(read.has_value());
        EXPECT_EQ(read.value(), 5u);
        EXPECT_STREQ(buf, "hello");
    }
}

TEST_F(TestFile, test_file_move_assignment_closes_previous)
{
    File first;
    File second;
    ASSERT_TRUE(first.open_tmpfile());
    ASSERT_TRUE(second.open_tmpfile());
    const int previous = second.fd();
    const int transferred = first.fd();
    second = std::move(first);
    EXPECT_FALSE(os::fd_type(previous));
    EXPECT_EQ(second.fd(), transferred);
    EXPECT_FALSE(first.is_open());
}

TEST_F(TestFile, test_file_write_string_view)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "sv.txt");
    {
        File file(path, "w");
        ASSERT_TRUE(file.is_open());
        std::string_view sv = "test data";
        auto wrote = file.write(sv);
        ASSERT_TRUE(wrote.has_value());
        EXPECT_EQ(wrote.value(), sv.size());
    }

    {
        File file(path, "r");
        ASSERT_TRUE(file.is_open());
        char buf[32] = {};
        auto read = file.read(buf, sizeof(buf));
        ASSERT_TRUE(read.has_value());
        EXPECT_EQ(read.value(), 9u);
        EXPECT_STREQ(buf, "test data");
    }
}

TEST_F(TestFile, test_file_seek_tell)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "seek.txt");
    {
        File file(path, "w");
        (void)file.write("abcdefgh", 8);
    }

    File file(path, "r");
    ASSERT_TRUE(file.is_open());

    auto told = file.tell();
    ASSERT_TRUE(told.has_value());
    EXPECT_EQ(told.value(), 0);
    EXPECT_TRUE(file.seek_begin(3).has_value());
    told = file.tell();
    ASSERT_TRUE(told.has_value());
    EXPECT_EQ(told.value(), 3);

    char buf[4] = {};
    auto read = file.read(buf, 3);
    ASSERT_TRUE(read.has_value());
    EXPECT_STREQ(buf, "def");
}

TEST_F(TestFile, test_large_file)
{
    if constexpr (build::is_emscripten)
        GTEST_SKIP() << "memfs allocates what it truncates";

    TmpDir tmp;
    ASSERT_TRUE(tmp);

    const std::string path = fs::combine(tmp.path(), "large.bin");
    const int64_t large_size = 5LL * 1024 * 1024 * 1024;
    ASSERT_TRUE(fs::write_binary(path, "").has_value());
    ASSERT_TRUE(fs::truncate(path, large_size).has_value());

    File file(path, "r+b");
    ASSERT_TRUE(file.is_open());

    const auto size = file.file_size();
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(size.value(), large_size);

    ASSERT_TRUE(file.seek_end(-1).has_value());
    const auto end_position = file.tell();
    ASSERT_TRUE(end_position.has_value());
    EXPECT_EQ(end_position.value(), large_size - 1);

    ASSERT_TRUE(file.seek_begin(large_size - 2).has_value());
    ASSERT_TRUE(file.write("x", 1).has_value());
    ASSERT_TRUE(file.seek(-1).has_value());

    std::string written;
    const auto read = file.read(written, 1);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(written, "x");
}

TEST_F(TestFile, test_file_size)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "size.txt");
    {
        File file(path, "w");
        (void)file.write("12345", 5);
    }

    File file(path, "r");
    auto size = file.file_size();
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(size.value(), 5);

    char buf[6] = {};
    auto read = file.read(buf, 5);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read.value(), 5u);
    EXPECT_STREQ(buf, "12345");
}

TEST_F(TestFile, test_file_eof)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "eof.txt");
    {
        File file(path, "w");
        (void)file.write("ab", 2);
    }

    File file(path, "r");
    char buf[16] = {};
    (void)file.read(buf, sizeof(buf));
    EXPECT_TRUE(file.eof());
}

TEST_F(TestFile, test_file_move)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "move.txt");
    File f1(path, "w");
    ASSERT_TRUE(f1.is_open());

    File f2(std::move(f1));
    EXPECT_TRUE(f2.is_open());
    EXPECT_FALSE(f1.is_open());
}

TEST_F(TestFile, test_file_open_nonexistent)
{
    File file;
    EXPECT_FALSE(file.is_open());
    EXPECT_FALSE(file.open(fs::combine(fs::tmp_path(), "sihd_nonexistent_12345/nope.txt"), "r").has_value());
    EXPECT_FALSE(file.is_open());
}

TEST_F(TestFile, test_file_open_mem_needs_buffer_size)
{
    File file;

    auto opened = file.open_mem("r", "data");

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::not_initialized);
    EXPECT_FALSE(file.is_open());
}

TEST_F(TestFile, test_file_buffer_size_kept_while_open)
{
    File file;
    ASSERT_TRUE(file.set_buffer_size(16).has_value());
    ASSERT_TRUE(file.open_mem("r", "hello").has_value());

    auto resized = file.set_buffer_size(4096);

    ASSERT_FALSE(resized.has_value());
    EXPECT_EQ(resized.error().code, ErrorCode::invalid_argument);
    std::string read;
    ASSERT_TRUE(file.read(read, 5).has_value());
    EXPECT_EQ(read, "hello");
}

TEST_F(TestFile, test_file_open_mem_keeps_bytes_after_nul)
{
    const std::string content("ab\0cde", 6);
    File file;
    ASSERT_TRUE(file.set_buffer_size(16).has_value());
    ASSERT_TRUE(file.open_mem("r", content).has_value());

    std::string read;
    ASSERT_TRUE(file.read(read, content.size()).has_value());

    EXPECT_EQ(read, content);
}

TEST_F(TestFile, test_file_failed_open_keeps_previous_file)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string path = fs::combine(tmp_dir.path(), "kept.txt");
    ASSERT_TRUE(fs::write(path, "kept").has_value());
    File file;
    ASSERT_TRUE(file.open(path, "r").has_value());

    EXPECT_FALSE(file.open(fs::combine(tmp_dir.path(), "missing/nope.txt"), "r").has_value());

    ASSERT_TRUE(file.is_open());
    EXPECT_EQ(file.path(), path);
    std::string read;
    ASSERT_TRUE(file.read(read, 4).has_value());
    EXPECT_EQ(read, "kept");
}

TEST_F(TestFile, test_file_reopen_truncates_after_previous_writes)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string path = fs::combine(tmp_dir.path(), "reopened.txt");
    File file;
    ASSERT_TRUE(file.open(path, "w").has_value());
    ASSERT_TRUE(file.write("abc", 3).has_value());

    ASSERT_TRUE(file.open(path, "w").has_value());
    ASSERT_TRUE(file.write("z", 1).has_value());
    ASSERT_TRUE(file.close().has_value());

    EXPECT_EQ(fs::read_all(path).value_or(""), "z");
}

TEST_F(TestFile, test_file_open_options_write_modes)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string path = fs::combine(tmp_dir.path(), "options.txt");
    ASSERT_TRUE(fs::write(path, "0123456789").has_value());

    const std::string in_place = content_after_write(path, {.write = true}, "ab");
    const std::string appended = content_after_write(path, {.write = true, .append = true}, "!");
    const std::string created_over = content_after_write(path, {.write = true, .create = true}, "cd");
    const std::string truncated = content_after_write(path, {.write = true, .truncate = true}, "z");
    const std::string created = content_after_write(fs::combine(tmp_dir.path(), "new.txt"),
                                                    {.write = true, .create = true, .exclusive = true},
                                                    "new");

    EXPECT_EQ(in_place, "ab23456789");
    EXPECT_EQ(appended, "ab23456789!");
    EXPECT_EQ(created_over, "cd23456789!");
    EXPECT_EQ(truncated, "z");
    EXPECT_EQ(created, "new");
}

TEST_F(TestFile, test_file_open_options_exclusive_refuses_an_existing_file)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string path = fs::combine(tmp_dir.path(), "existing.txt");
    ASSERT_TRUE(fs::write(path, "kept").has_value());
    File file;

    const auto opened = file.open(path, {.write = true, .create = true, .exclusive = true});

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::already_exists);
    EXPECT_FALSE(file.is_open());
    EXPECT_EQ(fs::read_all(path).value_or(""), "kept");
}

TEST_F(TestFile, test_file_open_options_refuses_misuse)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(tmp_dir);
    const std::string path = fs::combine(tmp_dir.path(), "misuse.txt");
    ASSERT_TRUE(fs::write(path, "kept").has_value());
    File file;
    ASSERT_TRUE(file.open(path, {.read = true}).has_value());

    const auto no_access = file.open(path, File::OpenOptions {});
    const auto read_truncate = file.open(path, {.read = true, .truncate = true});
    const auto read_append = file.open(path, {.read = true, .append = true});
    const auto exclusive_alone = file.open(path, {.write = true, .exclusive = true});

    ASSERT_FALSE(no_access.has_value());
    EXPECT_EQ(no_access.error().message, "a file opens to read, to write or both");
    ASSERT_FALSE(read_truncate.has_value());
    EXPECT_EQ(read_truncate.error().message, "truncate and append need write");
    ASSERT_FALSE(read_append.has_value());
    EXPECT_EQ(read_append.error().message, "truncate and append need write");
    ASSERT_FALSE(exclusive_alone.has_value());
    EXPECT_EQ(exclusive_alone.error().message, "exclusive needs create");
    std::string read;
    ASSERT_TRUE(file.read(read, 4).has_value());
    EXPECT_EQ(read, "kept");
}

TEST_F(TestFile, test_file_open_tmp)
{
    File file;
    EXPECT_TRUE(file.open_tmp(fs::combine(fs::tmp_path(), "sihd_test_"), true).has_value());
    EXPECT_TRUE(file.is_open());
    auto wrote = file.write("tmp", 3);
    ASSERT_TRUE(wrote.has_value());
    EXPECT_EQ(wrote.value(), 3u);
}

TEST_F(TestFile, test_file_read_to_string)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "str.txt");
    {
        File file(path, "w");
        (void)file.write("string_data", 11);
    }

    File file(path, "r");
    std::string str;
    auto read = file.read(str, 100);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read.value(), 11u);
    EXPECT_EQ(str, "string_data");
}

TEST_F(TestFile, test_file_read_line)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    const std::string path = fs::combine(tmp.path(), "lines.txt");
    ASSERT_TRUE(fs::write_binary(path, "one\ntwo;three").has_value());

    File file(path, "rb");
    ASSERT_TRUE(file.is_open());
    char *line = nullptr;
    size_t size = 0;

    auto read = file.read_line(&line, &size);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(std::string(line, *read), "one\n");

    read = file.read_line_delim(&line, &size, ';');
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(std::string(line, *read), "two;");

    read = file.read_line(&line, &size);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(std::string(line, *read), "three");

    read = file.read_line(&line, &size);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, 0u);

    free(line);
}

} // namespace test
