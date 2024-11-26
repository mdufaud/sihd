#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/clipboard.hpp>
#include <sihd/util/platform.hpp>

#if defined(SIHD_COMPILE_WITH_X11)
# include <X11/Xlib.h>
#endif

#if defined(SIHD_COMPILE_WITH_WAYLAND)
# include <wayland-client.h>
#endif

#if defined(__SIHD_WINDOWS__)
# include <windows.h>
#endif

namespace sihd::util::clipboard
{

SIHD_NEW_LOGGER("sihd::util::clipboard");

namespace
{

bool wayland_set_clipboard([[maybe_unused]] std::string_view str)
{
#if defined(SIHD_COMPILE_WITH_WAYLAND)
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
    Defer d([&display] { XCloseDisplay(display); });

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

    targets_atom = XInternAtom(display, "TARGETS", 0);
    text_atom = XInternAtom(display, "TEXT", 0);
    UTF8 = XInternAtom(display, "UTF8_STRING", 1);
    if (UTF8 == None)
        UTF8 = XA_STRING;
    Atom selection = XInternAtom(display, "CLIPBOARD", 0);

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
                    dummyEvent.type = ClientMessage;
                    dummyEvent.window
                        = XCreateSimpleWindow(display, DefaultRootWindow(display), 10, 10, 10, 10, 0, 0, 0);
                    dummyEvent.format = 32;
                    XSendEvent(display, dummyEvent.window, 0, 0, (XEvent *)&dummyEvent);
                    XFlush(display);
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

bool windows_set_clipboard([[maybe_unused]] std::string_view str)
{
#if defined(__SIHD_WINDOWS__)
    int characterCount;
    HANDLE object;
    WCHAR *buffer;

    characterCount = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, NULL, 0);
    if (!characterCount)
        return false;

    object = GlobalAlloc(GMEM_MOVEABLE, characterCount * sizeof(WCHAR));
    if (!object)
    {
        SIHD_LOG(error, "failed to allocate clipboard win32 handle");
        return false;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer)
    {
        SIHD_LOG(error, "failed to lock win32 handle");
        GlobalFree(object);
        return false;
    }

    MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, buffer, characterCount);
    GlobalUnlock(object);

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    int tries = 0;
    while (!OpenClipboard(0))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        tries++;

        if (tries == 3)
        {
            SIHD_LOG(error, "failed to open win32 clipboard");
            GlobalFree(object);
            return false;
        }
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, object);
    CloseClipboard();

    return true;
#else
    return false;
#endif
}

std::optional<std::string> wayland_get_clipboard()
{
#if defined(SIHD_COMPILE_WITH_WAYLAND)
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
    Defer d([&display] { XCloseDisplay(display); });

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

std::optional<std::string> windows_get_clipboard()
{
#if defined(__SIHD_WINDOWS__)
    HANDLE object;
    WCHAR *buffer;

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    int tries = 0;
    while (!OpenClipboard(0))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        tries++;

        if (tries == 3)
        {
            SIHD_LOG(error, "failed to open win32 clipboard");
            return std::nullopt;
        }
    }

    object = GetClipboardData(CF_UNICODETEXT);
    if (!object)
    {
        SIHD_LOG(error, "failed to convert win32 clipboard to string");
        CloseClipboard();
        return std::nullopt;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer)
    {
        SIHD_LOG(error, "failed to lock win32 handle");
        CloseClipboard();
        return std::nullopt;
    }

    std::wstring ws(buffer);

    GlobalUnlock(object);
    CloseClipboard();

    return std::string(ws.begin(), ws.end());
#else
    return std::nullopt;
#endif
}

} // namespace

bool set(std::string_view str)
{
    return windows_set_clipboard(str) || x11_set_clipboard(str) || wayland_set_clipboard(str);
}

std::optional<std::string> get()
{
    std::optional<std::string> ret;

    ret = windows_get_clipboard();
    if (!ret.has_value())
        ret = x11_get_clipboard();
    if (!ret.has_value())
        ret = wayland_get_clipboard();

    return ret;
}

} // namespace sihd::util::clipboard
