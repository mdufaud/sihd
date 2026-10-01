#include <iostream>

#include <gtest/gtest.h>

#include <sihd/sys/Process.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>
#include <sihd/zip/zip.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
SIHD_LOGGER;
using namespace sihd::zip;
using namespace sihd::util;
using namespace sihd::sys;
class TestZip: public ::testing::Test
{
    protected:
        TestZip() { sihd::util::LoggerManager::stream(); }

        virtual ~TestZip() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestZip, test_zip_tools)
{
    sihd::sys::TmpDir tmp_dir;

    const auto origin_path = fs::combine(tmp_dir.path(), "origin");
    const auto test_path = fs::combine(origin_path, "test");
    ASSERT_TRUE(fs::make_directories(test_path).has_value());
    ASSERT_TRUE(fs::make_directory(fs::combine(test_path, "one")).has_value());
    ASSERT_TRUE(fs::make_directory(fs::combine(test_path, "two")).has_value());
    ASSERT_TRUE(fs::write(fs::combine({test_path, "one", "file.txt"}), str::generate_random(10100)).has_value());

    const auto archive_path = fs::combine(tmp_dir.path(), "archive.zip");
    ASSERT_TRUE(sihd::zip::zip(test_path, archive_path).has_value());
    EXPECT_TRUE(fs::exists(archive_path));

    const auto unzipped_path = fs::combine(tmp_dir.path(), "unzipped");
    ASSERT_TRUE(sihd::zip::unzip(archive_path, unzipped_path).has_value());

    auto children_fs = fs::recursive_children(unzipped_path);
    for (auto & child : children_fs)
    {
        child = str::replace(child, "unzipped", "origin");
    }
    std::sort(children_fs.begin(), children_fs.end());

    auto origin_fs = fs::recursive_children(origin_path);
    std::sort(origin_fs.begin(), origin_fs.end());

    EXPECT_EQ(origin_fs, children_fs);
}

TEST_F(TestZip, test_list_entries)
{
    sihd::sys::TmpDir tmp_dir;

    const auto test_path = fs::combine(tmp_dir.path(), "data");
    ASSERT_TRUE(fs::make_directories(test_path).has_value());
    ASSERT_TRUE(fs::make_directory(fs::combine(test_path, "subdir")).has_value());
    ASSERT_TRUE(fs::write(fs::combine(test_path, "file1.txt"), "content1").has_value());
    ASSERT_TRUE(fs::write(fs::combine({test_path, "subdir", "file2.txt"}), "content2").has_value());

    const auto archive_path = fs::combine(tmp_dir.path(), "test.zip");
    ASSERT_TRUE(sihd::zip::zip(test_path, archive_path).has_value());

    auto entries = sihd::zip::list_entries(archive_path);
    EXPECT_FALSE(entries.empty());

    // check that known entries are present
    bool has_dir = false;
    bool has_file1 = false;
    bool has_file2 = false;
    for (const auto & entry : entries)
    {
        if (entry.find("subdir/") != std::string::npos && entry.back() == '/')
            has_dir = true;
        if (entry.find("file1.txt") != std::string::npos)
            has_file1 = true;
        if (entry.find("file2.txt") != std::string::npos)
            has_file2 = true;
    }
    EXPECT_TRUE(has_dir);
    EXPECT_TRUE(has_file1);
    EXPECT_TRUE(has_file2);
}

TEST_F(TestZip, test_list_entries_nonexistent)
{
    auto entries = sihd::zip::list_entries("/nonexistent/archive.zip");
    EXPECT_TRUE(entries.empty());
}

TEST_F(TestZip, test_unzip_invalid_archive)
{
    sihd::sys::TmpDir tmp_dir;
    const auto not_zip_path = fs::combine(tmp_dir.path(), "not_a_zip.zip");
    ASSERT_TRUE(fs::write(not_zip_path, "this is not a zip archive").has_value());

    auto res = sihd::zip::unzip(not_zip_path, fs::combine(tmp_dir.path(), "out"));
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, not_found);
    EXPECT_FALSE(res.error().message.empty());
}

} // namespace test
