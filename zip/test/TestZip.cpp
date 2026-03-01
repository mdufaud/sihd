#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>

#include <sihd/zip/zip.hpp>

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
    std::sort(children_fs.begin(), children_fs.end());

    auto origin_fs = fs::recursive_children(origin_path);
    std::sort(origin_fs.begin(), origin_fs.end());

    EXPECT_EQ(origin_fs, children_fs);
}

} // namespace test
