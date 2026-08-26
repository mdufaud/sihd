#include <sihd/sys/clipboard.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

// X11 and Wayland backends. They are compile-time options of this unix build
// (x11 / wayland scons opts); without them every operation is a no-op returning false/null.

#if defined(SIHD_COMPILE_WITH_X11)
# include <unistd.h> // unlink
# include <X11/Xlib.h>
#endif

#if defined(SIHD_COMPILE_WITH_WAYLAND)
# include <sihd/sys/fs.hpp>
# include <sihd/sys/proc.hpp>
#endif

namespace sihd::sys::clipboard
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::clipboard");

namespace
{

bool wayland_set_clipboard([[maybe_unused]] std::string_view str)
{
#if defined(SIHD_COMPILE_WITH_WAYLAND)
    proc::Options options({.timeout = std::chrono::milliseconds(200)});
    std::vector<std::string> args;
    args.reserve(2);
    args.emplace_back("wl-copy");
    args.emplace_back(str);
    auto exit_code = proc::execute(args, options);
    auto status = exit_code.wait_for(std::chrono::milliseconds(100));
    if (status == std::future_status::timeout)
        return false;
    return exit_code.get() == 0;
#endif
    return false;
}

bool x11_set_clipboard([[maybe_unused]] std::string_view str)
{
#if defined(SIHD_COMPILE_WITH_X11)
    Display *display;
    int screen;
    Window window;
    Atom targets_atom, text_atom, UTF8, XA_ATOM = 4, XA_STRING = 31;

    display = XOpenDisplay(nullptr);
    if (!display)
    {
        SIHD_LOG(error, "could not open X display");
        return false;
    }
    Defer defer_display([&display] { XCloseDisplay(display); });

    screen = DefaultScreen(display);
    window = XCreateSimpleWindow(display,
                                 RootWindow(display, screen),
                                 0,
                                 0,
                                 1,
                                 1,
                                 0,
                                 BlackPixel(display, screen),
                                 WhitePixel(display, screen));

    Defer defer_window([&display, &window] { XDestroyWindow(display, window); });

    targets_atom = XInternAtom(display, "TARGETS", False);
    text_atom = XInternAtom(display, "TEXT", False);
    UTF8 = XInternAtom(display, "UTF8_STRING", True);
    if (UTF8 == None)
        UTF8 = XA_STRING;
    Atom selection = XInternAtom(display, "CLIPBOARD", False);

    XEvent event;
    //   Window owner;
    XSetSelectionOwner(display, selection, window, 0);
    if (XGetSelectionOwner(display, selection) != window)
        return false;
    // Event loop
    for (;;)
    {
        XNextEvent(display, &event);
        switch (event.type)
        {
            case ClientMessage:
            {
                return true;
            }
            case SelectionRequest:
            {
                if (event.xselectionrequest.selection != selection)
                    break;
                XSelectionRequestEvent *xsr = &event.xselectionrequest;

                XSelectionEvent ev;
                memset(&ev, 0, sizeof(XSelectionEvent));
                ev.type = SelectionNotify;
                ev.display = xsr->display;
                ev.requestor = xsr->requestor;
                ev.selection = xsr->selection;
                ev.time = xsr->time;
                ev.target = xsr->target;
                ev.property = xsr->property;

                bool send_fake_event = false;

                int ret = 0;
                if (ev.target == targets_atom)
                {
                    ret = XChangeProperty(ev.display,
                                          ev.requestor,
                                          ev.property,
                                          XA_ATOM,
                                          32,
                                          PropModeReplace,
                                          (unsigned char *)&UTF8,
                                          1);
                }
                else if (ev.target == XA_STRING || ev.target == text_atom)
                {
                    ret = XChangeProperty(ev.display,
                                          ev.requestor,
                                          ev.property,
                                          XA_STRING,
                                          8,
                                          PropModeReplace,
                                          (unsigned char *)str.data(),
                                          str.size());
                    send_fake_event = true;
                }
                else if (ev.target == UTF8)
                {
                    ret = XChangeProperty(ev.display,
                                          ev.requestor,
                                          ev.property,
                                          UTF8,
                                          8,
                                          PropModeReplace,
                                          (unsigned char *)str.data(),
                                          str.size());
                    send_fake_event = true;
                }
                else
                {
                    ev.property = None;
                }

                if ((ret & 2) == 0)
                {
                    XSendEvent(display, ev.requestor, 0, 0, (XEvent *)&ev);
                }

                if (send_fake_event)
                {
                    // permits quitting event loop
                    XClientMessageEvent dummyEvent;
                    memset(&dummyEvent, 0, sizeof(XClientMessageEvent));
                    Window dummyWindow
                        = XCreateSimpleWindow(display, DefaultRootWindow(display), 10, 10, 10, 10, 0, 0, 0);
                    dummyEvent.type = ClientMessage;
                    dummyEvent.window = dummyWindow;
                    dummyEvent.format = 32;
                    XSendEvent(display, dummyWindow, False, 0, (XEvent *)&dummyEvent);
                    XFlush(display);
                    XDestroyWindow(display, dummyWindow);
                }

                break;
            }
            case SelectionClear:
                return false;
        }
    }
#endif
    return false;
}

std::optional<std::string> wayland_get_clipboard()
{
#if defined(SIHD_COMPILE_WITH_WAYLAND)
    std::string clipboard;
    proc::Options options(
        {.timeout = std::chrono::milliseconds(200),
         .stdout_callback = [&clipboard](std::string_view stdout_str) { clipboard += stdout_str; }});
    auto exit_code = proc::execute({"wl-paste", "--type", "text"}, options);
    auto status = exit_code.wait_for(std::chrono::milliseconds(100));
    if (status != std::future_status::timeout && exit_code.get() == 0)
    {
        return clipboard;
    }
#endif
    return std::nullopt;
}

std::optional<std::string> x11_get_clipboard()
{
#if defined(SIHD_COMPILE_WITH_X11)
    Display *display;
    Window owner, target_window, root;
    int screen;
    Atom sel, target_property, utf8;
    XEvent ev;
    XSelectionEvent *sev;

    display = XOpenDisplay(nullptr);
    if (!display)
    {
        SIHD_LOG(error, "could not open X display");
        return std::nullopt;
    }
    Defer defer_display([&display] { XCloseDisplay(display); });

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    sel = XInternAtom(display, "CLIPBOARD", False);
    utf8 = XInternAtom(display, "UTF8_STRING", False);

    owner = XGetSelectionOwner(display, sel);
    if (owner == None)
    {
        SIHD_LOG(error, "x11 clipboard has no owner");
        return std::nullopt;
    }

    /* The selection owner will store the data in a property on this
     * window: */
    target_window = XCreateSimpleWindow(display, root, -10, -10, 1, 1, 0, 0, 0);
    Defer defer_window([&display, &target_window] { XDestroyWindow(display, target_window); });

    /* That's the property used by the owner. Note that it's completely
     * arbitrary. */
    target_property = XInternAtom(display, "PENGUIN", False);

    /* Request conversion to UTF-8. Not all owners will be able to
     * fulfill that request. */
    XConvertSelection(display, sel, utf8, target_property, target_window, CurrentTime);

    // Event loop
    for (;;)
    {
        XNextEvent(display, &ev);
        switch (ev.type)
        {
            case SelectionNotify:
                sev = (XSelectionEvent *)&ev.xselection;
                if (sev->property == None)
                {
                    SIHD_LOG(error, "x11 clipboard conversion could not be performed");
                    return std::nullopt;
                }
                else
                {
                    Atom da, incr, type;
                    int di;
                    unsigned long size, dul;
                    unsigned char *prop_ret = NULL;

                    /* Dummy call to get type and size. */
                    XGetWindowProperty(display,
                                       target_window,
                                       target_property,
                                       0,
                                       0,
                                       False,
                                       AnyPropertyType,
                                       &type,
                                       &di,
                                       &dul,
                                       &size,
                                       &prop_ret);
                    XFree(prop_ret);

                    incr = XInternAtom(display, "INCR", False);
                    if (type == incr)
                    {
                        SIHD_LOG(error, "x11 clipboard Data too large and INCR mechanism not implemented");
                        return std::nullopt;
                    }

                    XGetWindowProperty(display,
                                       target_window,
                                       target_property,
                                       0,
                                       size,
                                       False,
                                       AnyPropertyType,
                                       &da,
                                       &di,
                                       &dul,
                                       &dul,
                                       &prop_ret);

                    std::string ret((const char *)prop_ret, size);

                    XFree(prop_ret);

                    /* Signal the selection owner that we have successfully read the
                     * data. */
                    // XDeleteProperty(display, target_window, target_property);

                    return ret;
                }
                break;
        }
    }
#endif
    return std::nullopt;
}

// ============================================================================
// Image clipboard functions
// ============================================================================

bool wayland_set_clipboard_image([[maybe_unused]] const Bitmap & bitmap)
{
#if defined(SIHD_COMPILE_WITH_WAYLAND)
    // Generate BMP data in memory using Bitmap's method
    std::vector<uint8_t> bmp_data = bitmap.to_bmp_data();
    if (bmp_data.empty())
        return false;

    // Pass BMP data directly via stdin to wl-copy
    std::string stdin_data(reinterpret_cast<const char *>(bmp_data.data()), bmp_data.size());
    proc::Options options;
    options.timeout = std::chrono::milliseconds(500);
    options.to_stdin = std::move(stdin_data);

    auto exit_code = proc::execute({"wl-copy", "--type", "image/bmp"}, options);
    auto status = exit_code.wait_for(std::chrono::milliseconds(500));
    if (status == std::future_status::timeout)
        return false;
    return exit_code.get() == 0;
#endif
    return false;
}

bool x11_set_clipboard_image([[maybe_unused]] const Bitmap & bitmap)
{
#if defined(SIHD_COMPILE_WITH_X11)
    if (bitmap.empty())
        return false;

    // Serialize bitmap to BMP format using Bitmap's method
    std::vector<uint8_t> bmp_data = bitmap.to_bmp_data();
    if (bmp_data.empty())
        return false;

    Display *display = XOpenDisplay(nullptr);
    if (!display)
    {
        SIHD_LOG(error, "could not open X display");
        return false;
    }
    Defer defer_display([&display] { XCloseDisplay(display); });

    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display,
                                        RootWindow(display, screen),
                                        0,
                                        0,
                                        1,
                                        1,
                                        0,
                                        BlackPixel(display, screen),
                                        WhitePixel(display, screen));

    Defer defer_window([&display, &window] { XDestroyWindow(display, window); });

    Atom targets_atom = XInternAtom(display, "TARGETS", False);
    Atom image_bmp = XInternAtom(display, "image/bmp", False);
    Atom image_xbmp = XInternAtom(display, "image/x-bmp", False);
    Atom selection = XInternAtom(display, "CLIPBOARD", False);
    constexpr Atom XA_ATOM = 4;

    XSetSelectionOwner(display, selection, window, CurrentTime);
    if (XGetSelectionOwner(display, selection) != window)
    {
        SIHD_LOG(error, "failed to become clipboard owner");
        return false;
    }

    XEvent event;
    for (;;)
    {
        XNextEvent(display, &event);
        switch (event.type)
        {
            case ClientMessage:
            {
                return true;
            }
            case SelectionRequest:
            {
                if (event.xselectionrequest.selection != selection)
                    break;

                XSelectionRequestEvent *xsr = &event.xselectionrequest;

                XSelectionEvent ev;
                memset(&ev, 0, sizeof(XSelectionEvent));
                ev.type = SelectionNotify;
                ev.display = xsr->display;
                ev.requestor = xsr->requestor;
                ev.selection = xsr->selection;
                ev.time = xsr->time;
                ev.target = xsr->target;
                ev.property = xsr->property;

                bool send_fake_event = false;

                if (ev.target == targets_atom)
                {
                    // Respond with available targets
                    Atom targets[] = {targets_atom, image_bmp, image_xbmp};
                    XChangeProperty(ev.display,
                                    ev.requestor,
                                    ev.property,
                                    XA_ATOM,
                                    32,
                                    PropModeReplace,
                                    reinterpret_cast<unsigned char *>(targets),
                                    3);
                }
                else if (ev.target == image_bmp || ev.target == image_xbmp)
                {
                    // Respond with BMP data
                    XChangeProperty(ev.display,
                                    ev.requestor,
                                    ev.property,
                                    ev.target,
                                    8,
                                    PropModeReplace,
                                    bmp_data.data(),
                                    bmp_data.size());
                    send_fake_event = true;
                }
                else
                {
                    ev.property = None;
                }

                XSendEvent(display, ev.requestor, False, 0, reinterpret_cast<XEvent *>(&ev));

                if (send_fake_event)
                {
                    // Exit event loop after serving one request
                    XClientMessageEvent dummyEvent;
                    memset(&dummyEvent, 0, sizeof(XClientMessageEvent));
                    Window dummyWindow
                        = XCreateSimpleWindow(display, DefaultRootWindow(display), 10, 10, 10, 10, 0, 0, 0);
                    dummyEvent.type = ClientMessage;
                    dummyEvent.window = dummyWindow;
                    dummyEvent.format = 32;
                    XSendEvent(display, dummyWindow, False, 0, reinterpret_cast<XEvent *>(&dummyEvent));
                    XFlush(display);
                    XDestroyWindow(display, dummyWindow);
                }
                break;
            }
            case SelectionClear:
                return false;
        }
    }
#endif
    return false;
}

std::optional<Bitmap> wayland_get_clipboard_image()
{
#if defined(SIHD_COMPILE_WITH_WAYLAND)
    std::string tmp_path = fs::tmp_path() + "/sihd_clipboard_get.bmp";

    std::string bmp_data;
    proc::Options options;
    options.timeout = std::chrono::milliseconds(500);
    options.stdout_callback = [&bmp_data](std::string_view data) {
        bmp_data += data;
    };

    // Try to get image as BMP - wl-paste writes to stdout by default
    auto exit_code = proc::execute({"wl-paste", "--type", "image/bmp"}, options);
    auto status = exit_code.wait_for(std::chrono::milliseconds(500));

    if (status == std::future_status::timeout || exit_code.get() != 0 || bmp_data.empty())
        return std::nullopt;

    // Parse BMP directly from memory
    Bitmap bm;
    std::vector<uint8_t> bmp_bytes(bmp_data.begin(), bmp_data.end());
    if (bm.read_bmp_data(bmp_bytes))
        return bm;
#endif
    return std::nullopt;
}

std::optional<Bitmap> x11_get_clipboard_image()
{
#if defined(SIHD_COMPILE_WITH_X11)
    Display *display = XOpenDisplay(nullptr);
    if (!display)
    {
        SIHD_LOG(error, "could not open X display");
        return std::nullopt;
    }
    Defer defer_display([&display] { XCloseDisplay(display); });

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
    Atom target_property = XInternAtom(display, "SIHD_CLIPBOARD", False);

    // Try different image formats
    const char *image_formats[] = {"image/bmp", "image/png", "image/x-bmp", nullptr};

    Window owner = XGetSelectionOwner(display, clipboard);
    if (owner == None)
        return std::nullopt;

    Window target_window = XCreateSimpleWindow(display, root, -10, -10, 1, 1, 0, 0, 0);
    Defer defer_window([&display, &target_window] { XDestroyWindow(display, target_window); });

    for (int i = 0; image_formats[i] != nullptr; ++i)
    {
        Atom format = XInternAtom(display, image_formats[i], False);
        XConvertSelection(display, clipboard, format, target_property, target_window, CurrentTime);
        XFlush(display);

        // Wait for SelectionNotify with timeout
        XEvent ev;
        bool got_event = false;

        // Simple polling with timeout
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            if (XPending(display) > 0)
            {
                XNextEvent(display, &ev);
                if (ev.type == SelectionNotify)
                {
                    got_event = true;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!got_event)
            continue;

        XSelectionEvent *sev = &ev.xselection;
        if (sev->property == None)
            continue;

        // Get the data
        Atom type;
        int format_ret;
        unsigned long nitems, bytes_after;
        unsigned char *data = nullptr;

        if (XGetWindowProperty(display,
                               target_window,
                               target_property,
                               0,
                               LONG_MAX,
                               False,
                               AnyPropertyType,
                               &type,
                               &format_ret,
                               &nitems,
                               &bytes_after,
                               &data)
            == Success)
        {
            if (data && nitems > 0)
            {
                // Try to parse as BMP
                Bitmap bm;
                // For now, we'd need to save to temp file and read back
                // or implement in-memory BMP parsing
                // This is a simplified version - just check if it's BMP format
                if (nitems >= 2 && data[0] == 'B' && data[1] == 'M')
                {
                    std::string tmp_path = "/tmp/sihd_x11_clipboard.bmp";
                    FILE *f = fopen(tmp_path.c_str(), "wb");
                    if (f)
                    {
                        fwrite(data, 1, nitems, f);
                        fclose(f);
                        if (bm.read_bmp(tmp_path))
                        {
                            unlink(tmp_path.c_str());
                            XFree(data);
                            return bm;
                        }
                        unlink(tmp_path.c_str());
                    }
                }
                XFree(data);
            }
        }

        XDeleteProperty(display, target_window, target_property);
    }
#endif
    return std::nullopt;
}

} // namespace

bool set_text(std::string_view str)
{
    return x11_set_clipboard(str) || wayland_set_clipboard(str);
}

bool set_image(const Bitmap & bitmap)
{
    return x11_set_clipboard_image(bitmap) || wayland_set_clipboard_image(bitmap);
}

std::optional<std::string> get_text()
{
    std::optional<std::string> ret;

    ret = x11_get_clipboard();
    if (!ret.has_value())
        ret = wayland_get_clipboard();

    return ret;
}

std::optional<Bitmap> get_image()
{
    std::optional<Bitmap> ret;

    ret = x11_get_clipboard_image();
    if (!ret.has_value())
        ret = wayland_get_clipboard_image();

    return ret;
}

std::optional<Content> get_any()
{
    // Try image first
    if (auto img = get_image(); img.has_value())
        return Content {std::move(*img)};

    // Fall back to text
    if (auto txt = get_text(); txt.has_value())
        return Content {std::move(*txt)};

    return std::nullopt;
}

} // namespace sihd::sys::clipboard
