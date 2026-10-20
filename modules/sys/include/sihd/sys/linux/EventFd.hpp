#ifndef __SIHD_SYS_LINUX_EVENTFD_HPP__
#define __SIHD_SYS_LINUX_EVENTFD_HPP__

#include <cstdint>

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/sys/linux headers are linux only"
#endif

namespace sihd::sys::linux_
{

class EventFd
{
    public:
        static constexpr bool supported = sihd::util::build::is_linux && !sihd::util::build::is_emscripten;

        EventFd();
        EventFd(EventFd && other);
        EventFd & operator=(EventFd && other);
        ~EventFd();

        EventFd(const EventFd &) = delete;
        EventFd & operator=(const EventFd &) = delete;

        sihd::util::Result<void> open(bool nonblocking = true);
        void close();

        sihd::util::Result<void> notify(uint64_t count = 1);
        sihd::util::Result<uint64_t> read();

        bool is_open() const;
        int fd() const;
        operator int() const;

    private:
        int _fd;
};

} // namespace sihd::sys::linux_

#endif
