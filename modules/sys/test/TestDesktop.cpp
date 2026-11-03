#include <gtest/gtest.h>

#include <sihd/sys/desktop.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

class TestDesktop: public ::testing::Test
{
    protected:
        TestDesktop() { LoggerManager::stream(); }

        virtual ~TestDesktop() { LoggerManager::clear_loggers(); }

        static void expect_error(const Result<void> & result, ErrorCode code, const std::string & message)
        {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error().code, code);
            EXPECT_EQ(result.error().message, message);
        }
};

TEST_F(TestDesktop, test_open_uri_refuses_what_is_no_uri)
{
    expect_error(desktop::open_uri(""), ErrorCode::invalid_argument, "not a uri: ''");
    expect_error(desktop::open_uri("no/scheme"), ErrorCode::invalid_argument, "not a uri: 'no/scheme'");
    expect_error(desktop::open_uri("C:/windows"), ErrorCode::invalid_argument, "not a uri: 'C:/windows'");
}

TEST_F(TestDesktop, test_open_uri_refuses_a_file_uri)
{
    expect_error(desktop::open_uri("FILE:///tmp/a"),
                 ErrorCode::invalid_argument,
                 "a file:// uri goes through open_file: 'FILE:///tmp/a'");
}

TEST_F(TestDesktop, test_missing_file)
{
    expect_error(desktop::open_file("no_such_file.txt"), ErrorCode::not_found, "no such file: 'no_such_file.txt'");
    expect_error(desktop::reveal("no_such_file.txt"), ErrorCode::not_found, "no such file: 'no_such_file.txt'");
}

} // namespace test
