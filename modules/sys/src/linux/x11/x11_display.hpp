#ifndef __SIHD_SYS_X11_DISPLAY_HPP__
#define __SIHD_SYS_X11_DISPLAY_HPP__

// RAII display connection shared by the x11 clipboard and screenshot backends.
// Xlib's default error handler exits the process: ours logs and fails the call instead.

#include <sihd/util/Logger.hpp>

#include <X11/Xlib.h>

namespace sihd::sys::x11
{

SIHD_NEW_LOGGER("sihd::sys::x11");

inline int display_error_handler(Display *, XErrorEvent *event)
{
    SIHD_LOG(error,
             "x11: protocol error {} on request {} (serial {})",
             event->error_code,
             event->request_code,
             event->serial);
    return 0;
}

struct DisplayConnection
{
        DisplayConnection(): _previous_error_handler(XSetErrorHandler(display_error_handler))
        {
            this->display = XOpenDisplay(nullptr);
        }

        ~DisplayConnection()
        {
            if (this->display != nullptr)
                XCloseDisplay(this->display);
            XSetErrorHandler(_previous_error_handler);
        }

        DisplayConnection(const DisplayConnection &) = delete;
        DisplayConnection & operator=(const DisplayConnection &) = delete;

        Display *display;

    private:
        XErrorHandler _previous_error_handler;
};

} // namespace sihd::sys::x11

#endif
