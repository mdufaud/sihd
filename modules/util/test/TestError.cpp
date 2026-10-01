#include <cerrno>
#include <expected>
#include <system_error>

#include <gtest/gtest.h>

#include <sihd/util/Error.hpp>

using enum sihd::util::ErrorCode;

namespace test
{
using namespace sihd::util;

TEST(TestError, test_error_default)
{
    Error err;
    EXPECT_EQ(err.code, none);
    EXPECT_TRUE(err.message.empty());
}

TEST(TestError, test_error_expected)
{
    const std::expected<int, Error> ok = 42;
    EXPECT_TRUE(ok.has_value());
    EXPECT_EQ(ok.value(), 42);

    const std::expected<int, Error> err = std::unexpected(Error(not_found, "failed"));
    EXPECT_FALSE(err.has_value());
    EXPECT_EQ(err.error().code, not_found);
    EXPECT_EQ(err.error().message, "failed");

    const auto propagated = err.and_then([](int value) { return std::expected<int, Error>(value * 2); });
    EXPECT_FALSE(propagated.has_value());
    EXPECT_EQ(propagated.error().message, "failed");
}

TEST(TestError, test_error_errno_classification)
{
    EXPECT_EQ(error_errno(ENOENT), not_found);
    EXPECT_EQ(error_errno(0), none);
    EXPECT_EQ(error_errno(EACCES), permission_denied);
    EXPECT_EQ(error_errno(EINVAL), invalid_argument);
    EXPECT_EQ(error_errno(EAGAIN), would_block);
    EXPECT_EQ(error_errno(EINTR), interrupted);
    EXPECT_EQ(error_errno(ENOMEM), out_of_memory);
    EXPECT_EQ(error_errno(EEXIST), already_exists);
    EXPECT_EQ(error_errno(EBADF), closed);
    EXPECT_EQ(error_errno(EPIPE), closed);
    EXPECT_EQ(error_errno(ERANGE), overflow);
    EXPECT_EQ(error_errno(999999), unknown);
}

TEST(TestError, test_error_from_errc_classification)
{
    EXPECT_EQ(error_from(std::errc::invalid_argument), invalid_argument);
    EXPECT_EQ(error_from(std::errc::no_such_file_or_directory), not_found);
    EXPECT_EQ(error_from(std::errc::permission_denied), permission_denied);
    EXPECT_EQ(error_from(std::errc::timed_out), timeout);
    EXPECT_EQ(error_from(std::errc::operation_would_block), would_block);
    EXPECT_EQ(error_from(std::errc::interrupted), interrupted);
    EXPECT_EQ(error_from(std::errc::not_enough_memory), out_of_memory);
    EXPECT_EQ(error_from(std::errc::file_exists), already_exists);
    EXPECT_EQ(error_from(std::errc::result_out_of_range), overflow);
    EXPECT_EQ(error_from(std::errc::broken_pipe), closed);
    EXPECT_EQ(error_from(std::errc::owner_dead), unknown);
}

TEST(TestError, test_error_ctor)
{
    const Error err(timeout, "operation timed out");
    EXPECT_EQ(err.code, timeout);
    EXPECT_EQ(err.message, "operation timed out");
}

TEST(TestError, test_error_from_errno)
{
    errno = ENOENT;
    const Error err = Error::from_errno("could not open '{}'", "toto");
    EXPECT_EQ(err.code, not_found);
    EXPECT_EQ(err.message, fmt::format("could not open '{}': {}", "toto", std::generic_category().message(ENOENT)));

    // message() may clobber errno: set it for the second snapshot
    errno = ENOENT;
    const Error no_args = Error::from_errno("could not list interfaces");
    EXPECT_EQ(no_args.code, not_found);
    EXPECT_EQ(no_args.message, fmt::format("could not list interfaces: {}", std::generic_category().message(ENOENT)));
}

TEST(TestError, test_error_native_errc)
{
    const Error errc_site = Error(std::errc::no_such_file_or_directory,
                                  "could not remove '{}': {}",
                                  "toto",
                                  std::make_error_code(std::errc::no_such_file_or_directory).message());
    EXPECT_EQ(errc_site.code, not_found);
    EXPECT_FALSE(errc_site.message.empty());
}

} // namespace test
