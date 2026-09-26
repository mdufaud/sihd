#include <expected>

#include <gtest/gtest.h>

#include <sihd/util/Error.hpp>

namespace test
{
using namespace sihd::util;

TEST(TestError, test_error_default)
{
    Error err;
    EXPECT_EQ(err.code, 0);
    EXPECT_TRUE(err.message.empty());
}

TEST(TestError, test_error_expected)
{
    const std::expected<int, Error> ok = 42;
    EXPECT_TRUE(ok.has_value());
    EXPECT_EQ(ok.value(), 42);

    const std::expected<int, Error> err = std::unexpected(Error {.code = 5, .message = "failed"});
    EXPECT_FALSE(err.has_value());
    EXPECT_EQ(err.error().code, 5);
    EXPECT_EQ(err.error().message, "failed");

    const auto propagated = err.and_then([](int value) { return std::expected<int, Error>(value * 2); });
    EXPECT_FALSE(propagated.has_value());
    EXPECT_EQ(propagated.error().message, "failed");
}

} // namespace test
