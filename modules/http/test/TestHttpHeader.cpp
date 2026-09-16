#include <iostream>

#include <gtest/gtest.h>

#include <sihd/http/HttpHeader.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/term.hpp>

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
    // a space is not a separator
    EXPECT_FALSE(header.add_header_from_str("Accept text/html"));

    EXPECT_TRUE(header.add_header_from_str("Accept2: a"));
    EXPECT_TRUE(header.add_header_from_str("Accept3:a\r\n"));

    EXPECT_EQ(header.get("accept"), "text/html");
    EXPECT_EQ(header.find("Accept2"), "a");
    EXPECT_EQ(header.find("Accept3"), "a");
    EXPECT_EQ(header.find("Accept4"), "");
}

TEST_F(TestHttpHeader, test_header_param)
{
    const std::string_view content_type = "multipart/form-data; boundary=\"quoted boundary\"; charset=utf-8";

    EXPECT_EQ(header_param(content_type, "boundary").value_or(""), "quoted boundary");
    EXPECT_EQ(header_param(content_type, "charset").value_or(""), "utf-8");
    EXPECT_EQ(header_param(content_type, "BOUNDARY").value_or(""), "quoted boundary");

    EXPECT_FALSE(header_param(content_type, "missing").has_value());
    EXPECT_FALSE(header_param("multipart/form-data", "boundary").has_value());
    EXPECT_FALSE(header_param("multipart/form-data; boundary=", "boundary").has_value());
    EXPECT_FALSE(header_param("multipart/form-data; boundary", "boundary").has_value());
    EXPECT_FALSE(header_param("", "boundary").has_value());

    EXPECT_EQ(header_param("form-data; name=\"a=b\"", "name").value_or(""), "a=b");
    EXPECT_EQ(header_param("form-data; name= token ", "name").value_or(""), "token");
    // a ';' inside a quoted value does not end the parameter
    EXPECT_EQ(header_param("form-data; filename=\"a;b.txt\"", "filename").value_or(""), "a;b.txt");
}

TEST_F(TestHttpHeader, test_set_header_overwrites)
{
    HttpHeader header;

    header.set_header("x-count:", "1");
    EXPECT_EQ(header.find("x-count"), "1");
    header.set_header("X-Count", "2");
    EXPECT_EQ(header.find("x-count"), "2");
}

TEST_F(TestHttpHeader, test_content_length)
{
    HttpHeader header;

    header.set_content_length(42);
    EXPECT_EQ(header.content_length().value_or(0), 42u);

    header.set_header("content-length:", "-1");
    EXPECT_FALSE(header.content_length().has_value());
    header.set_header("content-length:", "0x10");
    EXPECT_FALSE(header.content_length().has_value());
    header.set_header("content-length:", "12a");
    EXPECT_FALSE(header.content_length().has_value());
    header.set_header("content-length:", " 12 ");
    EXPECT_EQ(header.content_length().value_or(0), 12u);
    header.remove_header("content-length");
    EXPECT_FALSE(header.content_length().has_value());
}

} // namespace test
