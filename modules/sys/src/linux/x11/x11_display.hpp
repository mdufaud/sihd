#ifndef __SIHD_SYS_X11_DISPLAY_HPP__
#define __SIHD_SYS_X11_DISPLAY_HPP__

// Xlib's default error handler exits the process: ours logs and fails the call instead

#include <sihd/util/Error.hpp>

#include <X11/Xlib.h>

namespace sihd::sys::x11
{

int display_error_handler(Display *, XErrorEvent *event);

// The failure of an unopened display connection.
sihd::util::Error open_error();

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
