#include <sihd/sys/screen.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::sys::screen
{

SIHD_NEW_LOGGER("sihd::sys::screen");

Result<Monitor> primary()
{
    Result<std::vector<Monitor>> res = monitors();
    SIHD_UNEXPECTED_RETURN(res)
    for (const Monitor & monitor : *res)
    {
        if (monitor.primary)
            return monitor;
    }
    return std::unexpected(Error(not_found, "no primary monitor"));
}

Result<Monitor> containing(int x, int y)
{
    Result<std::vector<Monitor>> res = monitors();
    SIHD_UNEXPECTED_RETURN(res)
    for (const Monitor & monitor : *res)
    {
        if (x >= monitor.x && x < monitor.x + monitor.width && y >= monitor.y && y < monitor.y + monitor.height)
            return monitor;
    }
    return std::unexpected(Error(not_found, "no monitor at ({},{})", x, y));
}

} // namespace sihd::sys::screen
