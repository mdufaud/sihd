#include "internal/desktop_notifier.hpp"

namespace sihd::sys::internal
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

Result<void> check_timeout(const std::optional<Duration> & timeout)
{
    if (timeout.has_value() && *timeout < Duration(0))
        return std::unexpected(Error(invalid_argument, "a desktop notification timeout cannot be negative"));
    return {};
}

} // namespace sihd::sys::internal
