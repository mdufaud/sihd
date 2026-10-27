#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <sys/epoll.h>
# include <unistd.h>

# include <array>
# include <cerrno>
# include <cstdint>
# include <cstring>
# include <stdexcept>

# if defined(__SIHD_EMSCRIPTEN__)
#  include <fcntl.h>
# else
#  include <sihd/sys/linux/EventFd.hpp>
# endif

# include <sihd/sys/EventPoll.hpp>
# include <sihd/util/Result.hpp>

namespace sihd::sys
{

using namespace sihd::util;

namespace
{

constexpr uint64_t wake_key = UINT64_MAX;

struct epoll_event make_epoll_event(uint64_t key, bool read, bool write)
{
    struct epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = (read ? EPOLLIN : 0u) | (write ? EPOLLOUT : 0u);
    event.data.u64 = key;
    return event;
}

# if defined(__SIHD_EMSCRIPTEN__)

// emscripten has epoll but no eventfd
class WakeUp
{
    public:
        ~WakeUp()
        {
            for (int fd : _pipe)
            {
                if (fd >= 0)
                    ::close(fd);
            }
        }

        Result<void> open()
        {
            if (::pipe2(_pipe.data(), O_NONBLOCK | O_CLOEXEC) < 0)
                return std::unexpected(Error::from_errno("could not create the wake up pipe"));
            return {};
        }

        int fd() const { return _pipe[0]; }

        void notify()
        {
            const char byte = 1;
            (void)::write(_pipe[1], &byte, 1);
        }

        void drain()
        {
            std::array<char, 64> buffer;
            while (::read(_pipe[0], buffer.data(), buffer.size()) > 0)
            {
            }
        }

    private:
        std::array<int, 2> _pipe = {-1, -1};
};

# else

class WakeUp
{
    public:
        Result<void> open() { return _eventfd.open(); }

        int fd() const { return _eventfd.fd(); }

        void notify() { (void)_eventfd.notify(); }

        void drain() { (void)_eventfd.read(); }

    private:
        sihd::sys::linux_::EventFd _eventfd;
};

# endif

} // namespace

struct EventPoll::Impl
{
        ~Impl()
        {
            if (epoll_fd >= 0)
                ::close(epoll_fd);
        }

        Result<void> open()
        {
            epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
            if (epoll_fd < 0)
                return std::unexpected(Error::from_errno("could not create epoll instance"));
            auto opened = wake.open();
            SIHD_UNEXPECTED_RETURN(opened);
            struct epoll_event wake_event = make_epoll_event(wake_key, true, false);
            if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wake.fd(), &wake_event) < 0)
                return std::unexpected(Error::from_errno("could not register the wake up fd in epoll"));
            return {};
        }

        int epoll_fd = -1;
        WakeUp wake;
};

EventPoll::EventPoll(): _impl(std::make_unique<Impl>())
{
    auto opened = _impl->open();
    if (!opened)
        throw std::runtime_error(opened.error().message);
}

EventPoll::~EventPoll()
{
    if (this->is_running())
    {
        this->stop();
        this->service_wait_stop();
    }
}

bool EventPoll::_watch(int fd, Interest interest, bool watched)
{
    struct epoll_event event = make_epoll_event(static_cast<uint32_t>(fd), interest.read, interest.write);
    const int op = watched ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    int ret = ::epoll_ctl(_impl->epoll_fd, op, fd, &event);
    // a closed then reused fd lost its registration (MOD), an unclosed one still holds it (ADD)
    if (ret < 0 && op == EPOLL_CTL_MOD && errno == ENOENT)
        ret = ::epoll_ctl(_impl->epoll_fd, EPOLL_CTL_ADD, fd, &event);
    else if (ret < 0 && op == EPOLL_CTL_ADD && errno == EEXIST)
        ret = ::epoll_ctl(_impl->epoll_fd, EPOLL_CTL_MOD, fd, &event);
    return ret >= 0;
}

void EventPoll::_unwatch(int fd)
{
    // best effort: a closed fd already left the epoll set
    (void)::epoll_ctl(_impl->epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
}

void EventPoll::_wake()
{
    _impl->wake.notify();
}

int EventPoll::_wait(int milliseconds_timeout, std::vector<PollEvent> & ready)
{
    // level triggered: what does not fit is reported by the next wait
    std::array<struct epoll_event, 64> events;
    const int ret = ::epoll_wait(_impl->epoll_fd, events.data(), (int)events.size(), milliseconds_timeout);
    for (int i = 0; i < ret; ++i)
    {
        if (events[i].data.u64 == wake_key)
        {
            _impl->wake.drain();
            continue;
        }
        const uint32_t flags = events[i].events;
        ready.push_back({.fd = static_cast<int>(events[i].data.u64),
                         .readable = (flags & (EPOLLIN | EPOLLPRI)) != 0,
                         .writable = (flags & EPOLLOUT) != 0,
                         .error = (flags & EPOLLERR) != 0,
                         .closed = (flags & EPOLLHUP) != 0});
    }
    return ret;
}

} // namespace sihd::sys

#endif
