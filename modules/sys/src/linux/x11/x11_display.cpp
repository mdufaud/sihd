#include "x11_display.hpp"

#include <sihd/sys/env.hpp>
#include <sihd/util/Logger.hpp>

#include "../internal/desktop_env.hpp"

using namespace sihd::util;

namespace sihd::sys::internal
{

bool x11_compiled()
{
    return true;
}

} // namespace sihd::sys::internal

namespace sihd::sys::x11
{

SIHD_NEW_LOGGER("sihd::sys::x11");

int display_error_handler(Display *, XErrorEvent *event)
{
    SIHD_LOG(error,
             "x11: protocol error {} on request {} (serial {})",
             event->error_code,
             event->request_code,
             event->serial);
    return 0;
}

Error open_error()
{
    if (!env::get("DISPLAY").has_value())
        return {ErrorCode::not_supported, "no x11 session: DISPLAY is unset"};
    return {ErrorCode::io_error, "could not open the display"};
}

} // namespace sihd::sys::x11
