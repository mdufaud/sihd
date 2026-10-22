#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>

#include <sihd/sys/console.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/Logger.hpp>

using namespace sihd::util;

namespace sihd::sys::console
{

SIHD_NEW_LOGGER("sihd::sys::console");

namespace
{

bool set_attr(int fd, const struct termios & term)
{
    while (::tcsetattr(fd, TCSANOW, &term) == -1)
    {
        if (errno != EINTR)
            return false;
    }
    return true;
}

} // namespace

Result<WindowSize> size(int fd)
{
    struct winsize ws = {};
    auto res = os::ioctl(fd, TIOCGWINSZ, &ws);
    SIHD_UNEXPECTED_RETURN(res);
    return WindowSize {ws.ws_col, ws.ws_row};
}

bool is_interactive()
{
#if defined(__ANDROID__)
    // the terminal bridge connects stdin to a pipe from the UI: isatty() is 0 even though the user can type
    struct stat st;
    if (fstat(fileno(stdin), &st) == 0 && S_ISFIFO(st.st_mode))
        return true;
#endif
    return ::isatty(fileno(stdin)) != 0;
}

void set_output_utf8() {}

struct RawMode::Impl
{
        int fd = -1;
        struct termios saved = {};
        bool applied = false;
};

RawMode::RawMode(int fd): _impl(std::make_unique<Impl>())
{
    struct termios term;
    if (::tcgetattr(fd, &term) == -1)
        return;
    _impl->fd = fd;
    _impl->saved = term;
    term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    term.c_oflag &= ~OPOST;
    term.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    term.c_cflag &= ~(CSIZE | PARENB);
    term.c_cflag |= CS8;
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    _impl->applied = set_attr(fd, term);
}

RawMode::~RawMode()
{
    if (_impl->applied)
        set_attr(_impl->fd, _impl->saved);
}

bool RawMode::valid() const
{
    return _impl->applied;
}

struct NoEcho::Impl
{
        int fd = -1;
        struct termios saved = {};
        bool applied = false;
};

NoEcho::NoEcho(int fd): _impl(std::make_unique<Impl>())
{
    struct termios term;
    if (::tcgetattr(fd, &term) == -1)
        return;
    _impl->fd = fd;
    _impl->saved = term;
    term.c_lflag &= ~ECHO;
    _impl->applied = set_attr(fd, term);
}

NoEcho::~NoEcho()
{
    if (_impl->applied)
        set_attr(_impl->fd, _impl->saved);
}

bool NoEcho::valid() const
{
    return _impl->applied;
}

Result<void> enable_vt(int fd)
{
    (void)fd;
    return {};
}

} // namespace sihd::sys::console
