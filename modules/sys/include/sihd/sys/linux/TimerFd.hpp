#ifndef __SIHD_SYS_LINUX_TIMERFD_HPP__
#define __SIHD_SYS_LINUX_TIMERFD_HPP__

#include <cstdint>
#include <optional>

#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/sys/linux headers are linux only"
#endif

namespace sihd::sys::linux_
{

class TimerFd
{
    public:
        static constexpr bool supported = sihd::util::build::is_linux && !sihd::util::build::is_emscripten;

        TimerFd();
        TimerFd(TimerFd && other);
        TimerFd & operator=(TimerFd && other);
        ~TimerFd();

        TimerFd(const TimerFd &) = delete;
        TimerFd & operator=(const TimerFd &) = delete;

        sihd::util::Result<void> open(bool nonblocking = true);
        void close();

        sihd::util::Result<void> set(sihd::util::Duration initial,
                                     std::optional<sihd::util::Duration> period = std::nullopt);
        sihd::util::Result<void> disarm();
        sihd::util::Result<uint64_t> wait();

        bool is_open() const;
        int fd() const;
        operator int() const;

    private:
        int _fd;
};

} // namespace sihd::sys::linux_

#endif
