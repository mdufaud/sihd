#include <unistd.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <vector>

// sihd headers first: Xlib poisons common identifiers (None, Status, True,
// False, ...) with its macros.
#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#include "../internal/deadline.hpp"
#include "clipboard.hpp"
#include "x11_display.hpp"
#include <X11/Xatom.h>
#include <X11/Xlib.h>

// X11 clipboard backend: selection ownership, serving TARGETS and owned
// targets. One-shot set. INCR transfers unsupported (selections ~16 MB max).

namespace sihd::sys::clipboard::x11
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::clipboard");

namespace
{

// One data payload served for one selection target.
struct TargetData
{
        Atom target;
        const unsigned char *data = nullptr;
        unsigned long size = 0;
};

// Display connection, requestor/owner window and poller used by one clipboard
// operation.
struct ClipboardSession;

// The server's current time: selection requests want a real timestamp, not
// CurrentTime (ICCCM). Changing a property on our own window hands it back
// through its PropertyNotify.
Time server_timestamp(ClipboardSession & session);

struct ClipboardSession
{
        sihd::sys::x11::DisplayConnection conn;
        Window window = 0;
        Atom property = 0;
        Atom selection = 0;
        Time timestamp = CurrentTime;
        Poll poll {1};

        bool open()
        {
            if (conn.display == nullptr)
            {
                SIHD_LOG(error, "x11: could not open the display");
                return false;
            }

            window = XCreateSimpleWindow(conn.display,
                                         DefaultRootWindow(conn.display),
                                         -10,
                                         -10,
                                         1,
                                         1,
                                         0,
                                         BlackPixel(conn.display, DefaultScreen(conn.display)),
                                         WhitePixel(conn.display, DefaultScreen(conn.display)));
            property = XInternAtom(conn.display, "SIHD_CLIPBOARD", False);
            selection = XInternAtom(conn.display, "CLIPBOARD", False);
            poll.set_read_fd(ConnectionNumber(conn.display));
            timestamp = server_timestamp(*this);
            return true;
        }

        void close_window()
        {
            if (window == 0)
                return;
            if (conn.display != nullptr)
                XDestroyWindow(conn.display, window);
            window = 0;
        }
};

// Polls the X connection until an event is queued, the deadline expires or the
// connection drops.
bool wait_for_x_event(ClipboardSession & session, Timestamp deadline)
{
    while (XPending(session.conn.display) == 0)
    {
        session.poll.poll(internal::poll_timeout_ms(deadline));
        if (session.poll.polling_error() || internal::steady_now() >= deadline)
            return false;
    }
    return true;
}

Time server_timestamp(ClipboardSession & session)
{
    Display *display = session.conn.display;
    const Atom atom = XInternAtom(display, "SIHD_TIMESTAMP", False);
    XSelectInput(display, session.window, PropertyChangeMask);
    unsigned char dummy = 0;
    XChangeProperty(display, session.window, atom, XA_INTEGER, 8, PropModeAppend, &dummy, 1);
    XFlush(display);

    const Timestamp deadline = internal::deadline_from_now(get_timeout);
    for (;;)
    {
        if (!wait_for_x_event(session, deadline))
            return CurrentTime;
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == PropertyNotify && event.xproperty.atom == atom)
            return event.xproperty.time;
    }
}

// Reads the property the owner wrote for us, chunk by chunk, into `out`.
bool read_selection_property(ClipboardSession & session, ArrByte & out)
{
    Display *display = session.conn.display;

    Atom type = None;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long bytes_after = 0;
    unsigned char *data = nullptr;

    // 4-byte units per round; INCR is unsupported.
    const long chunk_units = 0xFFFFFF;

    // XGetWindowProperty offsets and lengths are 4-byte units, not elements.
    for (long offset = 0;; offset += chunk_units)
    {
        if (XGetWindowProperty(display,
                               session.window,
                               session.property,
                               offset,
                               0xFFFFFF, // plenty for one round; INCR is unsupported anyway
                               False,
                               AnyPropertyType,
                               &type,
                               &format,
                               &nitems,
                               &bytes_after,
                               &data)
            != Success)
        {
            return false;
        }

        if (data != nullptr)
        {
            // First round only: an INCR type means the owner chunks, unsupported.
            if (offset == 0 && type == XInternAtom(display, "INCR", False))
            {
                SIHD_LOG(error, "x11: clipboard data too large, INCR transfers are not supported");
                XFree(data);
                return false;
            }
            // Xlib hands format-32 properties back as long arrays.
            const size_t item_size = format == 32 ? sizeof(long) : static_cast<size_t>(format) / 8;
            out.push_back(reinterpret_cast<const int8_t *>(data), static_cast<size_t>(nitems) * item_size);
            XFree(data);
        }

        if (bytes_after == 0)
            return true;
    }
}

// Requests `target` from `selection` and waits for the SelectionNotify before
// reading the converted data into `out`.
bool convert_selection(ClipboardSession & session, Atom target, ArrByte & out, Timestamp deadline)
{
    Display *display = session.conn.display;

    out.clear();
    XConvertSelection(display, session.selection, target, session.property, session.window, session.timestamp);
    XFlush(display);

    for (;;)
    {
        if (!wait_for_x_event(session, deadline))
            return false;

        XEvent event;
        XNextEvent(display, &event);
        if (event.type != SelectionNotify)
            continue;

        if (event.xselection.property == None)
            return false; // the owner refused the conversion

        return read_selection_property(session, out);
    }
}

// The X selection target serving a mime type: text mimes map to the standard
// text targets, any other mime type is the target name itself (image/png, ...).
Atom target_atom(Display *display, std::string_view mime_str)
{
    if (mime_str == mime::utf8_text)
        return XInternAtom(display, "UTF8_STRING", False);
    if (mime_str == mime::plain_text)
        return XA_STRING;
    return XInternAtom(display, std::string(mime_str).c_str(), False);
}

// The canonical mime type of an X selection target: the text targets map to
// mime types, any other target is its own mime type.
std::optional<std::string> target_mime(Display *display, Atom target)
{
    if (target == XA_STRING)
        return std::string(mime::plain_text);

    char *name = XGetAtomName(display, target);
    if (name == nullptr)
        return std::nullopt;
    std::string owned(name);
    XFree(name);

    // TEXT is latin-1 per the ICCCM: under text/plain, not utf-8.
    if (owned == "UTF8_STRING")
        return std::string(mime::utf8_text);
    if (owned == "TEXT")
        return std::string(mime::plain_text);
    return owned;
}

// Selection bookkeeping targets: part of the selection protocol, not content.
bool is_protocol_target(std::string_view target)
{
    for (std::string_view protocol_target : {"TARGETS", "MULTIPLE", "TIMESTAMP", "SAVE_TARGETS"})
    {
        if (target == protocol_target)
            return true;
    }
    return false;
}

// Reads the TARGETS property: the mime types the current selection owner
// supports, in canonical form.
std::optional<std::vector<std::string>> read_targets(ClipboardSession & session, Timestamp deadline)
{
    Display *display = session.conn.display;

    ArrByte data;
    if (!convert_selection(session, XInternAtom(display, "TARGETS", False), data, deadline) || data.empty())
        return std::nullopt;

    // TARGETS is a format-32 property: Xlib hands it back as a long array.
    std::vector<std::string> targets;
    const unsigned long *atoms = reinterpret_cast<const unsigned long *>(data.data());
    for (size_t i = 0; i < data.size() / sizeof(unsigned long); ++i)
    {
        auto mime_str = target_mime(display, static_cast<Atom>(atoms[i]));
        // Several targets (TEXT, UTF8_STRING) canonicalize to the same mime.
        if (mime_str.has_value() && !is_protocol_target(*mime_str)
            && std::find(targets.begin(), targets.end(), *mime_str) == targets.end())
            targets.push_back(std::move(*mime_str));
    }
    return targets;
}

// Owns the selection and serves requests until the first data target is
// fulfilled, within the deadline set by the platform dispatcher; false when
// another client takes it or nothing requests in time.
bool serve_selection_once(ClipboardSession & session,
                          const std::vector<Atom> & offered_targets,
                          const std::vector<TargetData> & data_targets,
                          Timestamp deadline)
{
    Display *display = session.conn.display;

    XSetSelectionOwner(display, session.selection, session.window, session.timestamp);
    if (XGetSelectionOwner(display, session.selection) != session.window)
    {
        SIHD_LOG(error, "x11: failed to become clipboard owner");
        return false;
    }
    XFlush(display);

    const Atom targets_atom = XInternAtom(display, "TARGETS", False);

    for (;;)
    {
        if (!wait_for_x_event(session, deadline))
        {
            SIHD_LOG(warning, "x11: no application requested the clipboard content in time, it will be lost");
            return false;
        }

        XEvent event;
        XNextEvent(display, &event);
        switch (event.type)
        {
            case SelectionRequest:
            {
                const XSelectionRequestEvent & request = event.xselectionrequest;
                if (request.selection != session.selection)
                    break;

                XSelectionEvent notify = {};
                notify.type = SelectionNotify;
                notify.display = request.display;
                notify.requestor = request.requestor;
                notify.selection = request.selection;
                notify.time = request.time;
                notify.target = request.target;
                notify.property = request.property;

                bool served = false;
                if (request.target == targets_atom)
                {
                    // XChangeProperty's count is an int.
                    if (offered_targets.size() <= static_cast<size_t>(std::numeric_limits<int>::max()))
                    {
                        XChangeProperty(display,
                                        request.requestor,
                                        request.property,
                                        XA_ATOM,
                                        32,
                                        PropModeReplace,
                                        reinterpret_cast<const unsigned char *>(offered_targets.data()),
                                        static_cast<int>(offered_targets.size()));
                    }
                    else
                    {
                        notify.property = None;
                    }
                }
                else
                {
                    const TargetData *data = nullptr;
                    for (const TargetData & target_data : data_targets)
                    {
                        if (target_data.target == request.target)
                        {
                            data = &target_data;
                            break;
                        }
                    }

                    if (data == nullptr || data->size > static_cast<unsigned long>(std::numeric_limits<int>::max()))
                    {
                        notify.property = None;
                    }
                    else
                    {
                        XChangeProperty(display,
                                        request.requestor,
                                        request.property,
                                        request.target,
                                        8,
                                        PropModeReplace,
                                        data->data,
                                        static_cast<int>(data->size));
                        served = true;
                    }
                }

                XSendEvent(display, request.requestor, False, 0, reinterpret_cast<XEvent *>(&notify));
                XFlush(display);

                if (served)
                    return true;
                break;
            }
            case SelectionClear:
                // Another client took the selection.
                return false;
            default:
                break;
        }
    }
}

} // namespace

bool set_raw(const std::vector<RawContentView> & contents, Timestamp deadline)
{
    if (contents.empty())
        return false;

    ClipboardSession session;
    if (!session.open())
        return false;
    Defer defer_close([&] { session.close_window(); });

    Display *display = session.conn.display;

    const Atom targets_atom = XInternAtom(display, "TARGETS", False);

    std::vector<Atom> offered_targets = {targets_atom};
    std::vector<TargetData> data_targets;
    for (const RawContentView & content : contents)
    {
        const Atom target = target_atom(display, content.mime);
        offered_targets.push_back(target);
        data_targets.push_back(
            {target, reinterpret_cast<const unsigned char *>(content.data.data()), content.data.size()});
    }

    return serve_selection_once(session, offered_targets, data_targets, deadline);
}

// Fetches the wanted mime types - every offered one when the wanted list is
// empty - in `wanted_mimes` order (offered order when empty), in one session.
std::vector<RawContent> get_wanted(const std::vector<std::string_view> & wanted_mimes)
{
    ClipboardSession session;
    if (!session.open())
        return {};
    Defer defer_close([&] { session.close_window(); });

    Display *display = session.conn.display;

    if (XGetSelectionOwner(display, session.selection) == None)
    {
        SIHD_LOG(debug, "x11: clipboard has no owner");
        return {};
    }

    // One budget for the whole read: targets listing and every mime fetch.
    const Timestamp deadline = internal::deadline_from_now(get_timeout);

    auto targets = read_targets(session, deadline);
    if (!targets.has_value())
        return {};

    // Mime types to fetch, in fetch order.
    std::vector<std::string> fetch_list;
    if (wanted_mimes.empty())
        fetch_list = *targets;
    else
    {
        for (std::string_view mime_str : wanted_mimes)
        {
            // Request only mimes the owner advertises.
            if (auto offered_mime = mime::match_offered(*targets, mime_str); offered_mime.has_value())
            {
                if (std::find(fetch_list.begin(), fetch_list.end(), *offered_mime) == fetch_list.end())
                    fetch_list.emplace_back(*offered_mime);
            }
        }
    }

    std::vector<RawContent> contents;
    for (const std::string & mime_str : fetch_list)
    {
        ArrByte out;
        if (!convert_selection(session, target_atom(display, mime_str), out, deadline) || out.empty())
            continue;
        contents.push_back(RawContent {mime_str, std::move(out)});
    }
    return contents;
}

std::vector<RawContent> get_raw()
{
    return get_wanted({});
}

std::vector<RawContent> get_raw(const std::vector<std::string_view> & wanted_mimes)
{
    return get_wanted(wanted_mimes);
}

} // namespace sihd::sys::clipboard::x11
