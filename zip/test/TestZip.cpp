#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Process.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>

#include <sihd/zip/zip.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::zip;
using namespace sihd::util;
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
    sihd::util::TmpDir tmp_dir;

    const auto origin_path = tmp_dir.path() + "/origin";
    const auto test_path = origin_path + "/test";
    ASSERT_TRUE(fs::make_directories(test_path));
    ASSERT_TRUE(fs::make_directory(test_path + "/one"));
    ASSERT_TRUE(fs::make_directory(test_path + "/two"));
    ASSERT_TRUE(fs::write(test_path + "/one/file.txt", str::generate_random(10100)));

    const auto archive_path = tmp_dir.path() + "/archive.zip";
    ASSERT_TRUE(sihd::zip::zip(test_path, archive_path));
    EXPECT_TRUE(fs::exists(archive_path));

    const auto unzipped_path = tmp_dir.path() + "/unzipped";
    ASSERT_TRUE(sihd::zip::unzip(archive_path, unzipped_path));

    auto children_fs = fs::recursive_children(unzipped_path);
    for (auto & child : children_fs)
    {
        child = str::replace(child, "unzipped", "origin");
    }
    EXPECT_EQ(fs::recursive_children(origin_path), children_fs);
}

} // namespace test
