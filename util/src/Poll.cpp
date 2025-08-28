// sterror errno
#include <cerrno>
#include <cstring>

#include <sihd/util/Logger.hpp>
#include <sihd/util/Poll.hpp>

#if !defined(__SIHD_WINDOWS__)
// getrlimit
# include <sys/resource.h>
# include <sys/time.h>
#endif

namespace sihd::util
{

SIHD_LOGGER;

namespace
{

int get_fd_index(const std::vector<struct pollfd> & lst_fds, int fd)
{
    if (fd < 0)
        return -1;
    size_t i = 0;
    while (i < lst_fds.size())
    {
        if ((int)lst_fds[i].fd == fd)
            return i;
        ++i;
    }
    return -1;
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
        // take existing fd
        if ((int)lst_fds[i].fd == fd)
        {
            idx = i;
            break;
        }
        // take first free fd
        if (idx < 0 && (int)lst_fds[i].fd == -1)
            idx = i;
        ++i;
    }
    if (idx < 0 && i < max_fds)
    {
        // add new entry
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

Poll::Poll()
{
    _stop = false;
    _timeout_milliseconds = -1; // infinite block
    _max_fds = 0;
    _last_poll_time = 0;
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

void Poll::resize(int nfds)
{
    if (nfds < 0)
        return;
    std::lock_guard lock(_fds_mutex);
    _lst_fds.resize(nfds);
    _lst_events.reserve(nfds);
    int i = 0;
    while (i < nfds)
    {
        _lst_fds[i].events = 0;
        _lst_fds[i].revents = 0;
        _lst_fds[i].fd = -1;
        ++i;
    }
}

bool Poll::set_limit(int limit)
{
    if (limit < 0)
        _max_fds = os::max_fds();
    else
        _max_fds = limit;
    return true;
}

void Poll::clear_fds()
{
    std::lock_guard lock(_fds_mutex);
    _lst_fds.clear();
}

bool Poll::clear_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_fd_index(_lst_fds, fd);
    if (idx >= 0)
    {
        _lst_fds[idx].events = 0;
        _lst_fds[idx].revents = 0;
        _lst_fds[idx].fd = -1;
    }
    return idx >= 0;
}

size_t Poll::read_fds_size() const
{
    std::lock_guard lock(_fds_mutex);
    size_t ret = 0;
    size_t i = 0;
    while (i < _lst_fds.size())
    {
        ret += 1 * (_lst_fds[i].events & (POLLIN | POLLPRI));
        ++i;
    }
    return ret;
}

size_t Poll::write_fds_size() const
{
    std::lock_guard lock(_fds_mutex);
    size_t ret = 0;
    size_t i = 0;
    while (i < _lst_fds.size())
    {
        ret += 1 * (_lst_fds[i].events & POLLOUT);
        ++i;
    }
    return ret;
}

bool Poll::set_read_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_or_add_fd(_lst_fds, fd, _max_fds);
    if (idx >= 0)
    {
        _lst_fds[idx].fd = fd;
        _lst_fds[idx].events |= POLLIN;
    }
    return idx >= 0;
}

bool Poll::set_write_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_or_add_fd(_lst_fds, fd, _max_fds);
    if (idx >= 0)
    {
        _lst_fds[idx].fd = fd;
        _lst_fds[idx].events |= POLLOUT;
    }
    return idx >= 0;
}

bool Poll::rm_read_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_or_add_fd(_lst_fds, fd, _max_fds);
    if (idx >= 0)
    {
        _lst_fds[idx].events = (_lst_fds[idx].events & ~(POLLIN));
    }
    return idx >= 0;
}

bool Poll::rm_write_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = get_or_add_fd(_lst_fds, fd, _max_fds);
    if (idx >= 0)
    {
        _lst_fds[idx].events = (_lst_fds[idx].events & ~(POLLOUT));
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

bool Poll::on_start()
{
    if (_timeout_milliseconds < 0)
    {
        SIHD_LOG(error, "Poll: cannot start polling without a timeout");
        return false;
    }
    _stop = false;
    int ret = 0;
    while (ret >= 0 && _stop == false)
        ret = this->poll(_timeout_milliseconds);
    return ret >= 0;
}

int Poll::poll(int milliseconds_timeout)
{
    time_t before = _clock.now();
    int ret;
    {
        std::lock_guard lock(_fds_mutex);
#if !defined(__SIHD_WINDOWS__)
        ret = ::poll(_lst_fds.data(), _lst_fds.size(), milliseconds_timeout);
#else
        ret = ::WSAPoll(_lst_fds.data(), _lst_fds.size(), milliseconds_timeout);
#endif
    }
    _last_poll_time = _clock.now() - before;
    this->process_poll_results(ret);
    return ret;
}

void Poll::process_poll_results(int poll_return)
{
    _lst_events.clear();
    _timedout = poll_return == 0;
    _error = poll_return < 0;
    if (poll_return < 0)
        SIHD_LOG(error, "Poll: {}", os::last_error_str());
    if (poll_return > 0)
    {
        size_t i = 0;
        std::lock_guard lock_fds(_fds_mutex);
        while (poll_return > 0 && i < _lst_fds.size())
        {
            short revt = _lst_fds[i].revents;
            int fd = _lst_fds[i].fd;
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
    // also notify the timeout
    this->notify_observers(this);
}

} // namespace sihd::util
