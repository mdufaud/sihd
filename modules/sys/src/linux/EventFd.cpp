#include <cerrno>
#include <utility>

#include <sihd/sys/linux/EventFd.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
# include <sys/eventfd.h>
# include <unistd.h>
#endif

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::linux_
{

EventFd::EventFd(): _fd(-1) {}

EventFd::EventFd(EventFd && other): _fd(std::exchange(other._fd, -1)) {}

EventFd & EventFd::operator=(EventFd && other)
{
    if (this != &other)
    {
        this->close();
        _fd = std::exchange(other._fd, -1);
    }
    return *this;
}

EventFd::~EventFd()
{
    this->close();
}

bool EventFd::is_open() const
{
    return _fd != -1;
}

int EventFd::fd() const
{
    return _fd;
}

EventFd::operator int() const
{
    return _fd;
}

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)

Result<void> EventFd::open(bool nonblocking)
{
    this->close();
    const int fd = eventfd(0, EFD_CLOEXEC | (nonblocking ? EFD_NONBLOCK : 0));
    if (fd == -1)
        return std::unexpected(Error::from_errno("could not eventfd"));
    _fd = fd;
    return {};
}

void EventFd::close()
{
    if (_fd != -1)
    {
        ::close(_fd);
        _fd = -1;
    }
}

Result<void> EventFd::notify(uint64_t count)
{
    if (_fd == -1)
        return std::unexpected(Error(not_initialized, "eventfd is not open"));

    while (::write(_fd, &count, sizeof(count)) == -1)
    {
        if (errno != EINTR)
            return std::unexpected(Error::from_errno("could not write eventfd"));
    }
    return {};
}

Result<uint64_t> EventFd::read()
{
    if (_fd == -1)
        return std::unexpected(Error(not_initialized, "eventfd is not open"));

    uint64_t count = 0;
    while (::read(_fd, &count, sizeof(count)) == -1)
    {
        if (errno == EAGAIN)
            return 0;
        if (errno != EINTR)
            return std::unexpected(Error::from_errno("could not read eventfd"));
    }
    return count;
}

#else

Result<void> EventFd::open(bool)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

void EventFd::close()
{
    _fd = -1;
}

Result<void> EventFd::notify(uint64_t)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<uint64_t> EventFd::read()
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

#endif

} // namespace sihd::sys::linux_
