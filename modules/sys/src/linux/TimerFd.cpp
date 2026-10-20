#include <cerrno>
#include <utility>

#include <sihd/sys/linux/TimerFd.hpp>
#include <sihd/util/time.hpp>

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)
# include <sys/timerfd.h>
# include <unistd.h>
#endif

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys::linux_
{

TimerFd::TimerFd(): _fd(-1) {}

TimerFd::TimerFd(TimerFd && other): _fd(std::exchange(other._fd, -1)) {}

TimerFd & TimerFd::operator=(TimerFd && other)
{
    if (this != &other)
    {
        this->close();
        _fd = std::exchange(other._fd, -1);
    }
    return *this;
}

TimerFd::~TimerFd()
{
    this->close();
}

bool TimerFd::is_open() const
{
    return _fd != -1;
}

int TimerFd::fd() const
{
    return _fd;
}

TimerFd::operator int() const
{
    return _fd;
}

#if defined(__SIHD_LINUX__) && !defined(__SIHD_EMSCRIPTEN__)

Result<void> TimerFd::open(bool nonblocking)
{
    this->close();
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | (nonblocking ? TFD_NONBLOCK : 0));
    if (fd == -1)
        return std::unexpected(Error::from_errno("could not timerfd_create"));
    _fd = fd;
    return {};
}

void TimerFd::close()
{
    if (_fd != -1)
    {
        ::close(_fd);
        _fd = -1;
    }
}

Result<void> TimerFd::set(Duration initial, std::optional<Duration> period)
{
    if (_fd == -1)
        return std::unexpected(Error(not_initialized, "timerfd is not open"));
    if (initial <= Duration(0))
        return std::unexpected(Error(invalid_argument, "timerfd initial expiration must be positive"));

    struct itimerspec spec = {};
    spec.it_value = time::to_ts(initial);
    if (period)
        spec.it_interval = time::to_ts(*period);
    if (timerfd_settime(_fd, 0, &spec, nullptr) == -1)
        return std::unexpected(Error::from_errno("could not timerfd_settime"));
    return {};
}

Result<void> TimerFd::disarm()
{
    if (_fd == -1)
        return std::unexpected(Error(not_initialized, "timerfd is not open"));

    const struct itimerspec spec = {};
    if (timerfd_settime(_fd, 0, &spec, nullptr) == -1)
        return std::unexpected(Error::from_errno("could not timerfd_settime"));
    return {};
}

Result<uint64_t> TimerFd::wait()
{
    if (_fd == -1)
        return std::unexpected(Error(not_initialized, "timerfd is not open"));

    uint64_t expirations = 0;
    while (::read(_fd, &expirations, sizeof(expirations)) == -1)
    {
        if (errno == EAGAIN)
            return 0;
        if (errno != EINTR)
            return std::unexpected(Error::from_errno("could not read timerfd"));
    }
    return expirations;
}

#else

Result<void> TimerFd::open(bool)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

void TimerFd::close()
{
    _fd = -1;
}

Result<void> TimerFd::set(Duration, std::optional<Duration>)
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<void> TimerFd::disarm()
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

Result<uint64_t> TimerFd::wait()
{
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

#endif

} // namespace sihd::sys::linux_
