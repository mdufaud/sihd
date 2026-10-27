#include <sihd/sys/DesktopNotifier.hpp>

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

Result<void> DesktopNotifier::check_timeout(const std::optional<Duration> & timeout)
{
    if (timeout.has_value() && *timeout < Duration(0))
        return std::unexpected(Error(invalid_argument, "a desktop notification timeout cannot be negative"));
    return {};
}

} // namespace sihd::sys
