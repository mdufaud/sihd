#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/http/HttpSender.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::http;
using namespace sihd::util;
class TestHttpSender: public ::testing::Test
{
    protected:
        TestHttpSender() { sihd::util::LoggerManager::basic(); }

        virtual ~TestHttpSender() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestHttpSender, test_httpsender)
{
    auto http_response_opt = HttpSender::get("https://www.google.com");

    ASSERT_TRUE(http_response_opt.has_value());
    ASSERT_EQ(http_response_opt->status(), 200);

    for (const auto & [header, value] : http_response_opt->http_header().headers())
    {
        fmt::print("{}: {}\n", header, value);
    }
    fmt::print("{}\n", http_response_opt->content().cpp_str());
}

} // namespace test
