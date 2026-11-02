#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <unistd.h>

# include <utility>

# include <sihd/sys/linux/dbus/UnixFd.hpp>

namespace sihd::sys::linux_::dbus
{

UnixFd::UnixFd(int fd): _fd(fd) {}

UnixFd::UnixFd(UnixFd && other): _fd(std::exchange(other._fd, -1)) {}

UnixFd & UnixFd::operator=(UnixFd && other)
{
    if (this != &other)
    {
        this->_close();
        _fd = std::exchange(other._fd, -1);
    }
    return *this;
}

UnixFd::~UnixFd()
{
    this->_close();
}

int UnixFd::fd() const
{
    return _fd;
}

void UnixFd::_close()
{
    if (_fd >= 0)
        (void)::close(_fd);
    _fd = -1;
}

} // namespace sihd::sys::linux_::dbus

#endif
