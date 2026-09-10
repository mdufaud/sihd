#include <cstring>
#include <filesystem>

#include <gtest/gtest.h>

#include <sihd/sys/MappedFile.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/num.hpp>
#include <sihd/util/str.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::sys;

class TestMappedFile: public ::testing::Test
{
    protected:
        TestMappedFile() { sihd::util::LoggerManager::stream(); }

        virtual ~TestMappedFile() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!MappedFile::supported)
            {
                GTEST_SKIP() << "file mapping not supported on this platform";
            }
        }

        virtual void TearDown() {}
};

TEST_F(TestMappedFile, test_mappedfile_create)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(static_cast<bool>(tmp_dir));
    const std::string path = fs::combine(tmp_dir.path(), "mapped.bin");
    const size_t size = 4096;

    auto read_file = [](const std::string & p, size_t n) {
        std::string out(n, '\0');
        const ssize_t r = fs::read_binary(p, out.data(), n);
        return r == (ssize_t)n ? out : std::string();
    };

    std::string expected(size, '\0');
    for (size_t i = 0; i < size; ++i)
        expected[i] = (char)(i % 127);

    {
        MappedFile mapping;
        ASSERT_TRUE(mapping.create(path, size));
        EXPECT_EQ(mapping.size(), size);
        EXPECT_FALSE(mapping.read_only());
        EXPECT_NE(mapping.data(), nullptr);
        std::memcpy(mapping.data(), expected.data(), size);
        EXPECT_TRUE(mapping.sync());
        EXPECT_TRUE(mapping.clear());
        EXPECT_EQ(mapping.data(), nullptr);
        EXPECT_EQ(mapping.size(), 0u);
    }
    EXPECT_EQ(read_file(path, expected.size()), expected);

    {
        MappedFile read_only;
        ASSERT_TRUE(read_only.open_read_only(path));
        EXPECT_TRUE(read_only.read_only());
        EXPECT_EQ(read_only.size(), size);
        EXPECT_EQ(std::memcmp(read_only.cdata(), expected.data(), expected.size()), 0);
        EXPECT_TRUE(read_only.clear());
        // clear is idempotent
        EXPECT_TRUE(read_only.clear());
    }

    expected[0] = 'Z';
    {
        MappedFile read_write;
        ASSERT_TRUE(read_write.open_read_write(path));
        EXPECT_FALSE(read_write.read_only());
        ((char *)read_write.data())[0] = 'Z';
        EXPECT_TRUE(read_write.sync(true));
    }
    EXPECT_EQ(read_file(path, expected.size()), expected);

    // create truncates back to the requested size
    MappedFile truncated;
    ASSERT_TRUE(truncated.create(path, size / 2));
    EXPECT_EQ(truncated.size(), size / 2);
    truncated.clear();
    EXPECT_EQ(read_file(path, size / 2), std::string(size / 2, '\0'));
}

TEST_F(TestMappedFile, test_mappedfile_errors)
{
    TmpDir tmp_dir;
    ASSERT_TRUE(static_cast<bool>(tmp_dir));
    const std::string empty_path = fs::combine(tmp_dir.path(), "empty.bin");
    ASSERT_TRUE(fs::write(empty_path, ""));

    MappedFile mapping;
    EXPECT_FALSE(mapping.open_read_only(empty_path));
    EXPECT_FALSE(mapping.open_read_write(empty_path));
    EXPECT_FALSE(mapping.open_read_only(fs::combine(tmp_dir.path(), "nonexistent.bin")));
    EXPECT_FALSE(mapping.create(fs::combine(tmp_dir.path(), "zero.bin"), 0));
}

} // namespace test
