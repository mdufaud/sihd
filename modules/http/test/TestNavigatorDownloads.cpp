#include <gtest/gtest.h>

#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Logger.hpp>

#include "http_test_helpers.hpp"

namespace test
{

using namespace sihd::http;
using namespace sihd::util;
using namespace sihd::sys;

class TestNavigatorDownloads: public ::testing::Test
{
    protected:
        TestNavigatorDownloads() { LoggerManager::stream(); }
        virtual ~TestNavigatorDownloads() { LoggerManager::clear_loggers(); }
};

TEST_F(TestNavigatorDownloads, test_navigator_download_honors_max_response_size)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("big", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content(std::string(4096, 'x'));
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_max_response_size(64);

    sihd::sys::TmpDir dir;
    const std::string path = sihd::sys::fs::combine(dir.path(), "big.bin");
    auto resp = navigator.download(shard_url("localhost:3001/api/big"), path);

    ASSERT_FALSE(resp.has_value());
    EXPECT_EQ(resp.error().code, ErrorCode::overflow);
    EXPECT_FALSE(sihd::sys::fs::exists(path));
    EXPECT_EQ(sihd::sys::fs::children(dir.path()).size(), 0u);
}

TEST_F(TestNavigatorDownloads, test_navigator_download_failure_keeps_the_existing_file)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("missing", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_status(HttpStatus::NotFound);
        resp.set_plain_content("nope");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    sihd::sys::TmpDir dir;
    const std::string path = sihd::sys::fs::combine(dir.path(), "kept.txt");
    ASSERT_TRUE(sihd::sys::fs::write(path, "old").has_value());

    auto resp = navigator.download(shard_url("localhost:3001/api/missing"), path);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::NotFound);
    EXPECT_EQ(sihd::sys::fs::read_all(path).value_or("unreadable"), "old");
    EXPECT_EQ(sihd::sys::fs::children(dir.path()).size(), 1u);
}

TEST_F(TestNavigatorDownloads, test_navigator_download_replaces_the_file_without_leftovers)
{
    ServerScope scope;
    scope.server._webservice->set_entry_point("file", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("fresh");
    });

    scope.start();

    Navigator navigator;
    allow_local_targets(navigator);

    sihd::sys::TmpDir dir;
    const std::string path = sihd::sys::fs::combine(dir.path(), "replaced.txt");
    ASSERT_TRUE(sihd::sys::fs::write(path, "old").has_value());

    auto resp = navigator.download(shard_url("localhost:3001/api/file"), path);

    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(sihd::sys::fs::read_all(path).value_or("unreadable"), "fresh");
    EXPECT_EQ(sihd::sys::fs::children(dir.path()).size(), 1u);
}

} // namespace test
