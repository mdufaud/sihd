#include <gtest/gtest.h>

#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>

namespace test
{
using namespace sihd::sys;

class TestTmpDir: public ::testing::Test
{
    protected:
        TestTmpDir() = default;
        virtual ~TestTmpDir() = default;
        virtual void SetUp() {}
        virtual void TearDown() {}
};

TEST_F(TestTmpDir, test_tmpdir_creates_and_destroys)
{
    std::string saved_path;
    {
        TmpDir tmp;
        EXPECT_TRUE(tmp);
        EXPECT_FALSE(tmp.path().empty());
        EXPECT_TRUE(fs::is_dir(tmp.path()));
        saved_path = tmp.path();
    }
    EXPECT_FALSE(fs::exists(saved_path));
}

TEST_F(TestTmpDir, test_tmpdir_operators)
{
    TmpDir tmp;
    EXPECT_TRUE(tmp);

    std::string s = tmp;
    EXPECT_FALSE(s.empty());
    EXPECT_EQ(s, tmp.path());

    std::string_view sv = tmp;
    EXPECT_EQ(sv, tmp.path());
}

TEST_F(TestTmpDir, test_tmpdir_write_file_in_it)
{
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string path = fs::combine(tmp.path(), "hello.txt");
    EXPECT_TRUE(fs::write(path, "world"));
    EXPECT_TRUE(fs::exists(path));

    auto content = fs::read_all(path);
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(*content, "world");
}

} // namespace test
