#include "x11_display.hpp"

#include <sihd/util/Logger.hpp>

using namespace sihd::util;

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
    return {ErrorCode::io_error, "could not open the display"};
}

} // namespace sihd::sys::x11
