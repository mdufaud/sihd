#define SIHD_LOGGING_OFF 1

#include <expected>

#include <gtest/gtest.h>

#include <sihd/util/Error.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;

namespace test
{

// compile-check: with logging off every macro exists, expands to nothing, and SIHD_UNEXPECTED_LOG keeps its answer
TEST(TestLoggerOff, test_logging_off)
{
    SIHD_NEW_LOGGER("test::off");

    std::expected<int, sihd::util::Error> ok = 42;
    ASSERT_FALSE(SIHD_UNEXPECTED_LOG(ok));
    std::expected<int, sihd::util::Error> err = std::unexpected(sihd::util::Error(invalid_argument, "off"));
    ASSERT_TRUE(SIHD_UNEXPECTED_LOG(err));

    SIHD_LOG(info, "invisible {}", 42);
    SIHD_LOG_LVL(error, "invisible");
    SIHD_LOG_FORMAT(info, "invisible %d", 42);
    SIHD_TRACE("invisible");
    SIHD_TRACEL("invisible {}", 1);
    SIHD_TRACEV(42);
    SIHD_TRACE_FORMAT("invisible %d", 42);
    SIHD_COUT("invisible");
    SIHD_COUTV(invisible);
    SIHD_CERR("invisible {}", 42);
    // expands to nothing: an invalid format string must not even be instantiated with logging off
    SIHD_LOG(info, "bad {");
}

} // namespace test
