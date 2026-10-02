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

TEST(TestError, test_error_errno_socket_classification)
{
    EXPECT_EQ(error_errno(ECONNREFUSED), closed);
    EXPECT_EQ(error_errno(ETIMEDOUT), timeout);
    EXPECT_EQ(error_errno(ENOTCONN), closed);
    EXPECT_EQ(error_errno(ENETDOWN), closed);
    EXPECT_EQ(error_errno(ENETUNREACH), closed);
    EXPECT_EQ(error_errno(EHOSTUNREACH), closed);
    EXPECT_EQ(error_errno(EINPROGRESS), would_block);
    EXPECT_EQ(error_errno(EALREADY), would_block);
    EXPECT_EQ(error_errno(EISCONN), io_error);
    EXPECT_EQ(error_errno(ENOTSOCK), invalid_argument);
    EXPECT_EQ(error_errno(EADDRINUSE), already_exists);
    EXPECT_EQ(error_errno(EADDRNOTAVAIL), invalid_argument);
    EXPECT_EQ(error_errno(EPROTO), io_error);
    EXPECT_EQ(error_errno(EMSGSIZE), overflow);
    EXPECT_EQ(error_errno(EMFILE), out_of_memory);
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

TEST(TestError, test_error_errc_socket_classification)
{
    EXPECT_EQ(error_from(std::errc::connection_refused), closed);
    EXPECT_EQ(error_from(std::errc::network_down), closed);
    EXPECT_EQ(error_from(std::errc::network_reset), closed);
    EXPECT_EQ(error_from(std::errc::network_unreachable), closed);
    EXPECT_EQ(error_from(std::errc::host_unreachable), closed);
    EXPECT_EQ(error_from(std::errc::already_connected), io_error);
    EXPECT_EQ(error_from(std::errc::connection_already_in_progress), would_block);
    EXPECT_EQ(error_from(std::errc::message_size), overflow);
    EXPECT_EQ(error_from(std::errc::value_too_large), overflow);
    EXPECT_EQ(error_from(std::errc::address_not_available), invalid_argument);
    EXPECT_EQ(error_from(std::errc::destination_address_required), invalid_argument);
    EXPECT_EQ(error_from(std::errc::too_many_files_open), out_of_memory);
}

TEST(TestError, test_error_retryable)
{
    EXPECT_TRUE(Error(would_block, "").retryable());
    EXPECT_TRUE(Error(timeout, "").retryable());
    EXPECT_TRUE(Error(interrupted, "").retryable());
    EXPECT_TRUE(Error(error_errno(EAGAIN), "").retryable());
    EXPECT_TRUE(Error(error_errno(EINTR), "").retryable());
    EXPECT_FALSE(Error(not_found, "").retryable());
    EXPECT_FALSE(Error(closed, "").retryable());
    EXPECT_FALSE(Error().retryable());
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

TEST(TestError, test_error_unexpected_return_ctx)
{
    std::expected<int, Error> res = std::unexpected(Error(not_found, "no such channel"));
    EXPECT_FALSE(res.has_value());

    // on error: same code, old message first, context appended
    std::expected<int, Error> out = [&]() -> std::expected<int, Error> {
        SIHD_UNEXPECTED_RETURN_CTX(res, "in container '{}'", "node");
        return res;
    }();
    EXPECT_FALSE(out.has_value());
    EXPECT_EQ(out.error().code, not_found);
    EXPECT_EQ(out.error().message, "no such channel: in container 'node'");

    // on success: no return, args unevaluated
    std::expected<int, Error> ok = 42;
    std::expected<int, Error> passed = [&]() -> std::expected<int, Error> {
        SIHD_UNEXPECTED_RETURN_CTX(ok, "in container '{}'", "node");
        return ok;
    }();
    EXPECT_TRUE(passed.has_value());
    EXPECT_EQ(passed.value(), 42);

    // no variadic args: message kept as-is with the "{}: " prefix
    std::expected<int, Error> no_args = std::unexpected(Error(io_error, "could not read"));
    std::expected<int, Error> plain = [&]() -> std::expected<int, Error> {
        SIHD_UNEXPECTED_RETURN_CTX(no_args, "reading config");
        return no_args;
    }();
    EXPECT_EQ(plain.error().code, io_error);
    EXPECT_EQ(plain.error().message, "could not read: reading config");
}

} // namespace test
