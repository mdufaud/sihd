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
