#include <cstring>

#include <gtest/gtest.h>

#include <sihd/sys/File.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>

namespace test
{
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
        File f(path, "w");
        ASSERT_TRUE(f.is_open());
        EXPECT_EQ(f.write("hello", 5), 5);
    }

    {
        File f(path, "r");
        ASSERT_TRUE(f.is_open());
        char buf[16] = {};
        EXPECT_EQ(f.read(buf, sizeof(buf)), 5);
        EXPECT_STREQ(buf, "hello");
    }
}

TEST_F(TestFile, test_file_write_string_view)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "sv.txt");
    {
        File f(path, "w");
        ASSERT_TRUE(f.is_open());
        std::string_view sv = "test data";
        EXPECT_EQ(f.write(sv), (ssize_t)sv.size());
    }

    {
        File f(path, "r");
        ASSERT_TRUE(f.is_open());
        char buf[32] = {};
        ssize_t n = f.read(buf, sizeof(buf));
        EXPECT_EQ(n, 9);
        EXPECT_STREQ(buf, "test data");
    }
}

TEST_F(TestFile, test_file_seek_tell)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "seek.txt");
    {
        File f(path, "w");
        f.write("abcdefgh", 8);
    }

    File f(path, "r");
    ASSERT_TRUE(f.is_open());

    EXPECT_EQ(f.tell(), 0);
    f.seek_begin(3);
    EXPECT_EQ(f.tell(), 3);

    char buf[4] = {};
    f.read(buf, 3);
    EXPECT_STREQ(buf, "def");
}

TEST_F(TestFile, test_file_size)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "size.txt");
    {
        File f(path, "w");
        f.write("12345", 5);
    }

    File f(path, "r");
    EXPECT_EQ(f.file_size(), 5);
}

TEST_F(TestFile, test_file_eof)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "eof.txt");
    {
        File f(path, "w");
        f.write("ab", 2);
    }

    File f(path, "r");
    char buf[16] = {};
    f.read(buf, sizeof(buf));
    EXPECT_TRUE(f.eof());
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
    File f;
    EXPECT_FALSE(f.is_open());
    EXPECT_FALSE(f.open("/tmp/sihd_nonexistent_12345/nope.txt", "r"));
    EXPECT_FALSE(f.is_open());
}

TEST_F(TestFile, test_file_open_tmp)
{
    File f;
    EXPECT_TRUE(f.open_tmp("/tmp/sihd_test_", true));
    EXPECT_TRUE(f.is_open());
    EXPECT_EQ(f.write("tmp", 3), 3);
}

TEST_F(TestFile, test_file_read_to_string)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "str.txt");
    {
        File f(path, "w");
        f.write("string_data", 11);
    }

    File f(path, "r");
    std::string str;
    ssize_t n = f.read(str, 100);
    EXPECT_EQ(n, 11);
    EXPECT_EQ(str, "string_data");
}

} // namespace test
