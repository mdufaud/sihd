#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>
#include <sihd/wifi/connection.hpp>

namespace test
{
using namespace sihd::util;
namespace wifi = sihd::wifi;

// no daemon must be reached here: the checks run before any backend
class TestConnection: public ::testing::Test
{
    protected:
        TestConnection() { LoggerManager::stream(); }

        virtual ~TestConnection() { LoggerManager::clear_loggers(); }

        inline static const Duration timeout = Duration(time::sec(1));
};

TEST_F(TestConnection, test_connect_rejects_an_ssid_length)
{
    auto empty = wifi::connect("sihd-no-such-interface", "", timeout);
    auto empty_with_passphrase = wifi::connect("sihd-no-such-interface", "", "a long passphrase", timeout);
    auto too_long = wifi::connect("sihd-no-such-interface", std::string(33, 'a'), timeout);

    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(empty.error().message, "an ssid holds 1 to 32 bytes");
    ASSERT_FALSE(empty_with_passphrase.has_value());
    EXPECT_EQ(empty_with_passphrase.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(empty_with_passphrase.error().message, "an ssid holds 1 to 32 bytes");
    ASSERT_FALSE(too_long.has_value());
    EXPECT_EQ(too_long.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(too_long.error().message, "an ssid holds 1 to 32 bytes");
}

TEST_F(TestConnection, test_connect_rejects_a_passphrase_length)
{
    auto too_short = wifi::connect("sihd-no-such-interface", "home", "1234567", timeout);
    auto too_long = wifi::connect("sihd-no-such-interface", "home", std::string(64, 'a'), timeout);

    ASSERT_FALSE(too_short.has_value());
    EXPECT_EQ(too_short.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(too_short.error().message, "a passphrase holds 8 to 63 characters");
    ASSERT_FALSE(too_long.has_value());
    EXPECT_EQ(too_long.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(too_long.error().message, "a passphrase holds 8 to 63 characters");
}

} // namespace test
