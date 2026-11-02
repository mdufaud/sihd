#ifndef __SIHD_SYS_LINUX_DBUS_UNIXFD_HPP__
#define __SIHD_SYS_LINUX_DBUS_UNIXFD_HPP__

#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/sys/linux headers are linux only"
#endif

namespace sihd::sys::linux_::dbus
{

class UnixFd
{
    public:
        explicit UnixFd(int fd);
        UnixFd(UnixFd && other);
        UnixFd & operator=(UnixFd && other);
        ~UnixFd();

        UnixFd(const UnixFd &) = delete;
        UnixFd & operator=(const UnixFd &) = delete;

        int fd() const;

    private:
        void _close();

        int _fd;
};

} // namespace sihd::sys::linux_::dbus

#endif
