#include <sys/stat.h>
#include <unistd.h>

#include <sihd/util/platform.hpp>

// for get_max_rss / peak_rss
#if defined(__SIHD_WINDOWS__)

# include <debugapi.h>
# include <direct.h> // _stat
# include <psapi.h>
# include <winsock2.h>

# include <winsock.h>
# include <ws2def.h>

# include <windows.h>

#elif defined(__SIHD_APPLE__)

# include <mach/mach.h>

#elif defined(__SIHD_AIX__) || defined(__SIHD_SUN__)

# include <fcntl.h>
# include <procfs.h>

#endif

#if defined(__SIHD_LINUX__)

# include <dlfcn.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <sys/time.h>
# include <sys/wait.h>

# if defined(SIHD_COMPILE_WITH_X11)
#  include <X11/Xlib.h>
# endif

# if defined(SIHD_COMPILE_WITH_WAYLAND)
#  include <wayland-client.h>
# endif

# if !defined(__SIHD_EMSCRIPTEN__)
#  include <sys/ptrace.h>
# endif

#endif

// backtrace not available in windows / android / emscripten
#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# include <execinfo.h>
#endif

// for usage of sighandler_t in windows
#if defined(__SIHD_WINDOWS__)
typedef void (*sighandler_t)(int);
#endif

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>

#if !defined(__SIHD_UTIL_OS_DEFAULT_MAX_FDS__)
// backup for max_fds
# define __SIHD_UTIL_OS_DEFAULT_MAX_FDS__ 512
#endif

namespace sihd::util::os
{

SIHD_NEW_LOGGER("sihd::util::os");

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
        SIHD_LOG(error, "OS: could not open X display");
        return false;
    }
    Defer d([&] { XCloseDisplay(display); });

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
        SIHD_LOG(error, "OS: could not open X display");
        return std::nullopt;
    }
    Defer d([&] { XCloseDisplay(display); });

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    sel = XInternAtom(display, "CLIPBOARD", False);
    utf8 = XInternAtom(display, "UTF8_STRING", False);

    owner = XGetSelectionOwner(display, sel);
    if (owner == None)
    {
        SIHD_LOG(error, "OS: x11 clipboard has no owner");
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
                    SIHD_LOG(error, "OS: x11 clipboard conversion could not be performed");
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
                        SIHD_LOG(error, "OS: x11 clipboard Data too large and INCR mechanism not implemented");
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

} // namespace

pid_t pid()
{
#if !defined(__SIHD_WINDOWS__)
    return getpid();
#else
    return GetCurrentProcessId();
#endif
}

rlim_t max_fds()
{
    if constexpr (os::is_emscripten)
    {
        return __SIHD_UTIL_OS_DEFAULT_MAX_FDS__;
    }
#if !defined(__SIHD_WINDOWS__)
    struct rlimit r;
    if (getrlimit(RLIMIT_NOFILE, &r) == -1)
    {
        SIHD_LOG(error, "OS: getrlim_t {}", strerror(errno));
        return 0;
    }
    return r.rlim_cur;
#else
    return __SIHD_UTIL_OS_DEFAULT_MAX_FDS__;
#endif
}

bool ioctl(int fd, unsigned long request, void *arg_ptr, bool logerror)
{
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::ioctl(fd, request, arg_ptr) == 0;
#else
    bool ret = ::ioctlsocket(fd, request, reinterpret_cast<long unsigned int *>(arg_ptr)) == 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: ioctl error: {}", strerror(errno));
    return ret == 0;
}

bool stat(const char *pathname, struct stat *statbuf, bool logerror)
{
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::stat(pathname, statbuf) == 0;
#else
    bool ret = ::_stat(pathname, reinterpret_cast<struct _stat *>(statbuf)) == 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: stat error: {}", strerror(errno));
    return ret == 0;
}

bool fstat(int fd, struct stat *statbuf, bool logerror)
{
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::fstat(fd, statbuf) == 0;
#else
    bool ret = ::_fstat(fd, reinterpret_cast<struct _stat *>(statbuf)) == 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: fstat error: {}", strerror(errno));
    return ret == 0;
}

bool setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot setsockopt on a negative socket");
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::setsockopt(socket, level, optname, optval, optlen) >= 0;
#else
    bool ret = ::setsockopt(socket, level, optname, (const char *)optval, optlen) >= 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: {}", strerror(errno));
    return ret;
}

bool getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen, bool logerror)
{
    if (socket < 0)
        throw std::runtime_error("OS: cannot getsockopt on a negative socket");
#if !defined(__SIHD_WINDOWS__)
    bool ret = ::getsockopt(socket, level, optname, optval, optlen) >= 0;
#else
    bool ret = ::getsockopt(socket, level, optname, (char *)optval, optlen) >= 0;
#endif
    if (!ret && logerror)
        SIHD_LOG(error, "OS: getsockopt error: {}", strerror(errno));
    return ret;
}

bool is_root()
{
#if defined(__SIHD_WINDOWS__)
# pragma message("TODO os::is_root")
    return false;
#else
    return getuid() == 0;
#endif
}

// backtrace not available in windows / android

#if !defined(__SIHD_WINDOWS__) && !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)

# ifndef SIHD_MAX_BACKTRACE_SIZE
#  define SIHD_MAX_BACKTRACE_SIZE 50
# endif

# ifndef SIHD_DEFAULT_BACKTRACE_SIZE
#  define SIHD_DEFAULT_BACKTRACE_SIZE 15
# endif

namespace
{

ssize_t write(int fd, const char *s)
{
    int i = 0;
    while (s[i])
        ++i;
    return ::write(fd, s, i);
}

ssize_t write_endl(int fd, const char *s)
{
    ssize_t ret = write(fd, s);
    return ret + ::write(fd, "\n", 1);
}

ssize_t write_number(int fd, int number)
{
    char c;
    if (number < 10)
    {
        c = (char)number + '0';
        return ::write(fd, &c, 1);
    }
    ssize_t ret = write_number(fd, number / 10);
    c = (char)(number % 10) + '0';
    return ret + ::write(fd, &c, 1);
}

static void *backtrace_buffer[SIHD_MAX_BACKTRACE_SIZE];

} // namespace

ssize_t backtrace(int fd, size_t backtrace_size)
{
    size_t wanted_size = std::min(backtrace_size, (size_t)SIHD_MAX_BACKTRACE_SIZE);
    size_t size = ::backtrace(backtrace_buffer, wanted_size);
    char **strings = (char **)backtrace_symbols(backtrace_buffer, size);
    bool ret = write(fd, "backtrace (") > 0;
    ret = ret && write_number(fd, size) > 0;
    ret = ret && write_endl(fd, " calls)") > 0;
    if (strings == nullptr)
    {
        ret = ret && write_endl(fd, "Error while getting backtrace symbols");
        return -1;
    }
    uint32_t i = 0;
    while (ret && i < size)
    {
        ret = ret && write(fd, "[") > 0;
        ret = ret && write_number(fd, i) > 0;
        ret = ret && write(fd, "]\t--> ") > 0;
        ret = ret && write_endl(fd, strings[i]) > 0;
        ++i;
    }
    free(strings);
    return size;
}

#else // no backtrace

# pragma message("Backtrace is not supported for this platform")

ssize_t backtrace(int fd, size_t backtrace_size)
{
    (void)fd;
    (void)backtrace_size;
    return 0;
}

#endif // end of backtrace

// debuggers

bool is_run_by_valgrind()
{
    char *ldpreload = getenv("LD_PRELOAD");
    return ldpreload != nullptr
           && (strstr(ldpreload, "/valgrind/") != nullptr || strstr(ldpreload, "/vgpreload") != nullptr);
}

// https://stackoverflow.com/questions/3596781/how-to-detect-if-the-current-process-is-being-run-by-gdb

bool is_run_by_debugger()
{
#if defined(__SIHD_EMSCRIPTEN__)
    return false;
#elif !defined(__SIHD_WINDOWS__)
    char buf[4096];

    const int status_fd = open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
        return false;

    const ssize_t num_read = read(status_fd, buf, sizeof(buf) - 1);
    close(status_fd);

    if (num_read <= 0)
        return false;

    buf[num_read] = '\0';
    constexpr char tracerPidString[] = "TracerPid:";
    const auto tracer_pid_ptr = strstr(buf, tracerPidString);
    if (!tracer_pid_ptr)
        return false;

    for (const char *characterPtr = tracer_pid_ptr + sizeof(tracerPidString) - 1; characterPtr <= buf + num_read;
         ++characterPtr)
    {
        if (isspace(*characterPtr))
            continue;
        else
            return isdigit(*characterPtr) != 0 && *characterPtr != '0';
    }

    return false;
#else
    return IsDebuggerPresent();
#endif
}

/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 */

/**
 * Returns the peak (maximum so far) resident set size (physical
 * memory use) measured in bytes, or zero if the value cannot be
 * determined on this OS.
 */
ssize_t peak_rss()
{
    if constexpr (os::is_emscripten)
    {
        return -1L;
    }
#if defined(__SIHD_WINDOWS__)

    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (ssize_t)info.PeakWorkingSetSize;

#elif defined(__SIHD_AIX__) || defined(__SIHD_SUN__)

    struct psinfo psinfo;
    int fd = -1;
    if ((fd = open("/proc/self/psinfo", O_RDONLY)) == -1)
    {
        SIHD_LOG(error, "OS: peak_rss open: {}", strerror(errno));
        return (ssize_t)-1L;
    }
    if (read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo))
    {
        SIHD_LOG(error, "OS: peak_rss read: {}", strerror(errno));
        close(fd);
        return (ssize_t)-1L;
    }
    close(fd);
    return (ssize_t)(psinfo.pr_rssize * 1024L);

#elif defined(__SIHD_LINUX__) || defined(__SIHD_APPLE__)

    struct rusage rusage;
    if (getrusage(RUSAGE_SELF, &rusage) == -1)
    {
        SIHD_LOG(error, "OS: peak_rss getrusage: {}", strerror(errno));
        return (ssize_t)-1L;
    }
# if defined(__SIHD_APPLE__)
    return (ssize_t)rusage.ru_maxrss;
# else
    return (ssize_t)(rusage.ru_maxrss * 1024L);
# endif

#else
# pragma message("os::peak_rss is not supported on this platform")
    return (ssize_t)-1L;
#endif
}

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
ssize_t current_rss()
{
    if constexpr (os::is_emscripten)
    {
        return -1L;
    }
#if defined(__SIHD_WINDOWS__)

    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (ssize_t)info.WorkingSetSize;

#elif defined(__SIHD_APPLE__)

    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) != KERN_SUCCESS)
        return (ssize_t)-1L;
    return (ssize_t)info.resident_size;

#elif defined(__SIHD_LINUX__)

    long rss = 0L;
    FILE *fp = NULL;
    if ((fp = fopen("/proc/self/statm", "r")) == NULL)
        return (ssize_t)-1L;
    if (fscanf(fp, "%*s%ld", &rss) != 1)
    {
        fclose(fp);
        return (ssize_t)-1L;
    }
    fclose(fp);
    return (ssize_t)rss * (ssize_t)sysconf(_SC_PAGESIZE);

#else
# pragma message("os::current_rss is not supported on this platform")
    return (ssize_t)-1L;
#endif
}

bool set_clipboard(std::string_view str)
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
        SIHD_LOG(error, "OS: failed to allocate clipboard win32 handle");
        return false;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer)
    {
        SIHD_LOG(error, "OS: failed to lock win32 handle");
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
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        tries++;

        if (tries == 3)
        {
            SIHD_LOG(error, "OS: failed to open win32 clipboard");
            GlobalFree(object);
            return false;
        }
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, object);
    CloseClipboard();

    return true;
#else
    return x11_set_clipboard(str) || wayland_set_clipboard(str);
#endif
}

std::optional<std::string> get_clipboard()
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
            SIHD_LOG(error, "OS: failed to open win32 clipboard");
            return std::nullopt;
        }
    }

    object = GetClipboardData(CF_UNICODETEXT);
    if (!object)
    {
        SIHD_LOG(error, "OS: failed to convert win32 clipboard to string");
        CloseClipboard();
        return std::nullopt;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer)
    {
        SIHD_LOG(error, "OS: failed to lock win32 handle");
        CloseClipboard();
        return std::nullopt;
    }

    std::wstring ws(buffer);

    GlobalUnlock(object);
    CloseClipboard();

    return std::string(ws.begin(), ws.end());
#else
    std::optional<std::string> opt_str = x11_get_clipboard();
    if (!opt_str)
        opt_str = wayland_get_clipboard();
    return opt_str;
#endif
}

} // namespace sihd::util::os
