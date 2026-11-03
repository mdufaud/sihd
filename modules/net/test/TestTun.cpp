#include <array>
#include <string>

#include <gtest/gtest.h>

#include <sihd/net/Tun.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::net;

class TestTun: public ::testing::Test
{
    protected:
        TestTun() { LoggerManager::stream(); }

        virtual ~TestTun() { LoggerManager::clear_loggers(); }
};

TEST_F(TestTun, test_tun_open_rejects_bad_names)
{
    Tun tun;

    auto empty = tun.open("");
    auto too_long = tun.open("sixteen_chars_ab");

    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error().code, ErrorCode::invalid_argument);
    ASSERT_FALSE(too_long.has_value());
    EXPECT_EQ(too_long.error().code, ErrorCode::invalid_argument);
    EXPECT_FALSE(tun.is_open());
}

TEST_F(TestTun, test_tun_closed_refuses_io)
{
    Tun tun;
    std::array<char, 16> packet {};

    auto read = tun.read(packet.data(), packet.size());
    const std::string payload = "packet";
    auto written = tun.write(payload);
    auto waited = tun.wait_readable(Duration(0));

    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, ErrorCode::not_initialized);
    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error().code, ErrorCode::not_initialized);
    ASSERT_FALSE(waited.has_value());
    EXPECT_EQ(waited.error().code, ErrorCode::not_initialized);
}

TEST_F(TestTun, test_tun_open_without_privileges_or_driver_leaves_it_closed)
{
    Tun tun;

    auto opened = tun.open("sihdtun0");

    if (opened)
        GTEST_SKIP() << "the test runs with the rights to create interfaces";
    EXPECT_TRUE(opened.error().code == ErrorCode::permission_denied || opened.error().code == ErrorCode::not_found)
        << opened.error().message;
    EXPECT_FALSE(tun.is_open());
}

TEST_F(TestTun, test_tun_adopt_and_fd_on_a_closed_tun)
{
    Tun tun;

    auto adopted = Tun::adopt(-1);
    auto fd = tun.fd();

    ASSERT_FALSE(adopted.has_value());
    ASSERT_FALSE(fd.has_value());
    if constexpr (build::is_windows)
    {
        EXPECT_EQ(adopted.error().code, ErrorCode::not_supported);
        EXPECT_EQ(fd.error().code, ErrorCode::not_supported);
    }
    else
    {
        EXPECT_EQ(adopted.error().code, ErrorCode::invalid_argument);
        EXPECT_EQ(fd.error().code, ErrorCode::not_initialized);
    }
}

TEST_F(TestTun, test_tun_moved_from_is_closed)
{
    Tun tun;
    Tun moved(std::move(tun));
    std::array<char, 16> packet {};

    auto read = tun.read(packet.data(), packet.size());

    EXPECT_FALSE(tun.is_open());
    EXPECT_FALSE(moved.is_open());
    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, ErrorCode::not_initialized);
}

} // namespace test
