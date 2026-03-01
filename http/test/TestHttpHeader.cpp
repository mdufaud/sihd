#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

#include <sihd/http/HttpHeader.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::http;
using namespace sihd::util;
class TestHttpHeader: public ::testing::Test
{
    protected:
        TestHttpHeader() { sihd::util::LoggerManager::stream(); }

        virtual ~TestHttpHeader() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestHttpHeader, test_httpheader)
{
    HttpHeader header;

    EXPECT_TRUE(header.add_header_from_str("Accept: text/html"));

    EXPECT_FALSE(header.add_header_from_str(""));
    EXPECT_FALSE(header.add_header_from_str("Accept"));
    EXPECT_FALSE(header.add_header_from_str("Accept:"));
    EXPECT_FALSE(header.add_header_from_str("Accept: "));

    EXPECT_TRUE(header.add_header_from_str("Accept2: a"));

    EXPECT_EQ(header.get("accept"), "text/html");
    EXPECT_EQ(header.find("Accept2"), "a");
    EXPECT_EQ(header.find("Accept3"), "");
}

} // namespace test
