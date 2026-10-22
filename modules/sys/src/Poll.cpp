#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ranges>
#include <thread>

#include <sihd/sys/Poll.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <poll.h>
# include <sys/resource.h>
#else
# include <winsock2.h>
#endif

#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

struct Poll::Impl
{
        std::vector<struct pollfd> fds;
};

namespace
{

int get_fd_index(const std::vector<struct pollfd> & lst_fds, int fd)
{
    if (fd < 0)
        return -1;
    const auto it = std::ranges::find_if(lst_fds, [fd](const struct pollfd & entry) { return (int)entry.fd == fd; });
    if (it == lst_fds.end())
        return -1;
    return (int)(it - lst_fds.begin());
}

int native_poll(std::vector<struct pollfd> & lst_fds, int milliseconds_timeout)
{
#if !defined(__SIHD_WINDOWS__)
    return ::poll(lst_fds.data(), lst_fds.size(), milliseconds_timeout);
#else
    // WSAPoll fails when no entry holds a socket, where poll waits the timeout out
    const bool has_socket = std::ranges::any_of(lst_fds,
                                                [](const struct pollfd & entry) { return entry.fd != INVALID_SOCKET; });
    if (!has_socket && milliseconds_timeout >= 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds_timeout));
        return 0;
    }
    return ::WSAPoll(lst_fds.data(), lst_fds.size(), milliseconds_timeout);
#endif
}

size_t count_fds_events(const std::vector<struct pollfd> & lst_fds, int mask)
{
    return std::ranges::count_if(lst_fds, [mask](const struct pollfd & entry) { return (entry.events & mask) != 0; });
}

int get_or_add_fd(std::vector<struct pollfd> & lst_fds, int fd, rlim_t max_fds)
{
    if (fd < 0)
        return -1;
    if (max_fds <= 0)
        SIHD_LOG(warning, "Poll: no max file descriptors limit was set");
    int idx = -1;
    size_t i = 0;
    while (i < lst_fds.size())
    {
        if ((int)lst_fds[i].fd == fd)
        {
            idx = i;
            break;
        }

        if (idx < 0 && (int)lst_fds[i].fd == -1)
            idx = i;
        ++i;
    }
    if (idx < 0 && i < max_fds)
    {
        struct pollfd p;
        p.fd = -1;
        p.events = 0;
        p.revents = 0;
        lst_fds.push_back(p);
        idx = i;
    }
    return idx;
}

} // namespace

Poll::Poll(): _impl(std::make_unique<Impl>())
{
    _stop = false;
    _timeout_milliseconds = -1;
    _max_fds = 0;
    _last_poll_time = Duration(0);
    _timedout = false;
    _error = false;
}

Poll::Poll(int limit): Poll()
{
    this->set_limit(limit);
}

Poll::~Poll()
{
    if (this->is_running())
    {
        this->stop();
        this->service_wait_stop();
    }
}

size_t Poll::fds_size() const
{
    std::lock_guard lock(_fds_mutex);
    return _impl->fds.size();
}

void Poll::resize(int nfds)
{
    if (nfds < 0)
        return;
    std::lock_guard lock(_fds_mutex);
    _impl->fds.resize(nfds);
    _lst_events.reserve(nfds);
    int i = 0;
    while (i < nfds)
    {
        _impl->fds[i].events = 0;
        _impl->fds[i].revents = 0;
        _impl->fds[i].fd = -1;
        ++i;
    }
}

bool Poll::set_limit(int limit)
{
    if (limit < 0)
    {
        auto fds = os::max_fds();
        SIHD_UNEXPECTED_LOG(fds);
        _max_fds = fds.value_or(0);
    }
    else
        _max_fds = limit;
    return true;
}

void Poll::clear_fds()
{
    std::lock_guard lock(_fds_mutex);
    _impl->fds.clear();
}

bool Poll::clear_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_fd_index(_impl->fds, fd);
    if (idx >= 0)
    {
        _impl->fds[idx].events = 0;
        _impl->fds[idx].revents = 0;
        _impl->fds[idx].fd = -1;
    }
    return idx >= 0;
}

size_t Poll::read_fds_size() const
{
    std::lock_guard lock(_fds_mutex);
    return count_fds_events(_impl->fds, POLLIN | POLLPRI);
}

size_t Poll::write_fds_size() const
{
    std::lock_guard lock(_fds_mutex);
    return count_fds_events(_impl->fds, POLLOUT);
}

bool Poll::set_read_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_or_add_fd(_impl->fds, fd, _max_fds);
    if (idx >= 0)
    {
        _impl->fds[idx].fd = fd;
        _impl->fds[idx].events |= POLLIN;
    }
    return idx >= 0;
}

bool Poll::set_write_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_or_add_fd(_impl->fds, fd, _max_fds);
    if (idx >= 0)
    {
        _impl->fds[idx].fd = fd;
        _impl->fds[idx].events |= POLLOUT;
    }
    return idx >= 0;
}

bool Poll::rm_read_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_fd_index(_impl->fds, fd);
    if (idx >= 0)
    {
        _impl->fds[idx].events = (_impl->fds[idx].events & ~(POLLIN));
        if (_impl->fds[idx].events == 0)
            _impl->fds[idx].fd = -1;
    }
    return idx >= 0;
}

bool Poll::rm_write_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_fd_index(_impl->fds, fd);
    if (idx >= 0)
    {
        _impl->fds[idx].events = (_impl->fds[idx].events & ~(POLLOUT));
        if (_impl->fds[idx].events == 0)
            _impl->fds[idx].fd = -1;
    }
    return idx >= 0;
}

bool Poll::set_timeout(int milliseconds)
{
    if (milliseconds < 0)
        return false;
    _timeout_milliseconds = milliseconds;
    return true;
}

bool Poll::on_stop()
{
    _stop = true;
    return true;
}

void Poll::request_stop()
{
    _stop = true;
}

bool Poll::on_start()
{
    if (_timeout_milliseconds < 0)
    {
        SIHD_LOG(error, "Poll: cannot start polling without a timeout");
        return false;
    }

    // a stop landing after set_ready must not be erased here
    _stop = false;

    this->service_set_ready();
    int ret = 0;
    while (_stop == false && ret >= 0)
        ret = this->poll(_timeout_milliseconds);
    return ret >= 0;
}

int Poll::poll(int milliseconds_timeout)
{
    Timestamp before = _clock.now();
    const time::UnixTime deadline = before + time::ms(std::max(milliseconds_timeout, 0));
    int ret;
    int saved_errno;
    {
        std::lock_guard lock(_fds_mutex);
        ret = native_poll(_impl->fds, milliseconds_timeout);
        saved_errno = os::last_socket_error();

        while (ret < 0 && error_errno(saved_errno) == ErrorCode::interrupted)
        {
            // rounded up: a truncated remainder would time out before the deadline
            const time::UnixTime remaining = std::max<time::UnixTime>(deadline - _clock.now().nanoseconds(), 0);
            ret = native_poll(_impl->fds,
                              milliseconds_timeout < 0 ? -1 : (int)time::to_ms(remaining + time::ms(1) - 1));
            saved_errno = os::last_socket_error();
        }
        _last_poll_time = _clock.now() - before;
        this->process_poll_results(ret, saved_errno);
    }
    this->notify_observers(this);
    return ret;
}

void Poll::process_poll_results(int poll_return, int saved_errno)
{
    _lst_events.clear();
    _timedout = poll_return == 0;
    _error = poll_return < 0;
    if (_error)
        SIHD_LOG(error, "Poll: {}", os::error_str(saved_errno));
    if (poll_return > 0)
    {
        size_t i = 0;
        while (poll_return > 0 && i < _impl->fds.size())
        {
            short revt = _impl->fds[i].revents;
            int fd = _impl->fds[i].fd;
            if (revt != 0)
            {
                PollEvent evt;
                evt.fd = fd;
#if defined(__SIHD_WINDOWS__)
                evt.closed = revt & (POLLHUP);
#else
                evt.closed = revt & (POLLHUP | POLLRDHUP);
#endif
                evt.error = revt & (POLLNVAL | POLLERR);
                evt.readable = revt & (POLLIN | POLLPRI);
                evt.writable = revt & POLLOUT;
                --poll_return;
                _lst_events.push_back(evt);
            }
            ++i;
        }
    }
}

} // namespace sihd::sys
