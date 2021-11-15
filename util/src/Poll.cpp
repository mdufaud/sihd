#include <sihd/util/Poll.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/OS.hpp>

// sterror errno
#include <string.h>
#include <errno.h>

#if !defined(__SIHD_WINDOWS__)
// getrlimit
# include <sys/time.h>
# include <sys/resource.h>
#endif

namespace sihd::util
{

LOGGER;

Poll::Poll()
{
    this->_init();
}

Poll::Poll(int limit)
{
    this->_init();
    this->set_limit(limit);
}

Poll::~Poll()
{
    this->stop();
    std::lock_guard lock(_run_mutex);
}

void    Poll::_init()
{
    _running = false;
    _timeout_milliseconds = -1; // infinite block
    _max_fds = 0;
    _last_poll_time = 0;
    _timedout = false;
    _error = false;
}

int     Poll::_get_or_add_fd(int fd)
{
    if (fd < 0)
        return -1;
    if (_max_fds <= 0)
        LOG(warning, "Poll: no max file descriptors limit was set");
    std::lock_guard lock(_fds_mutex);
    int idx = -1;
    size_t i = 0;
    while (i < _lst_fds.size())
    {
        // take existing fd
        if ((int)_lst_fds[i].fd == fd)
        {
            idx = i;
            break ;
        }
        // take first free fd
        if (idx < 0 && (int)_lst_fds[i].fd == -1)
            idx = i;
        ++i;
    }
    if (idx < 0 && i < _max_fds)
    {
        // add new entry
        struct pollfd p;
        p.fd = -1;
        p.events = 0;
        p.revents = 0;
        _lst_fds.push_back(p);
        idx = i;
    }
    return idx;
}

int    Poll::_get_fd_index(int fd)
{
    if (fd < 0)
        return -1;
    size_t i = 0;
    while (i < _lst_fds.size())
    {
        if ((int)_lst_fds[i].fd == fd)
            return i;
        ++i;
    }
    return -1;
}

void    Poll::resize(int nfds)
{
    if (nfds < 0)
        return ;
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

bool    Poll::set_limit(int limit)
{
    if (limit < 0)
        _max_fds = OS::get_max_fds();
    else
        _max_fds = limit;
    return true;
}

void    Poll::clear_fds()
{
    std::lock_guard lock(_fds_mutex);
    _lst_fds.clear();
}

bool    Poll::clear_fd(int fd)
{
    std::lock_guard lock(_fds_mutex);
    int idx = this->_get_fd_index(fd);
    if (idx >= 0)
    {
        _lst_fds[idx].events = 0;
        _lst_fds[idx].revents = 0;
        _lst_fds[idx].fd = -1;
    }
    return idx >= 0;
}

size_t  Poll::read_fds_size()
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

size_t  Poll::write_fds_size()
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

bool    Poll::set_read_fd(int fd)
{
    int idx = this->_get_or_add_fd(fd);
    if (idx >= 0)
    {
        _lst_fds[idx].fd = fd;
        _lst_fds[idx].events |= POLLIN;
    }
    return idx >= 0;
}

bool    Poll::set_write_fd(int fd)
{
    int idx = this->_get_or_add_fd(fd);
    if (idx >= 0)
    {
        _lst_fds[idx].fd = fd;
        _lst_fds[idx].events |= POLLOUT;
    }
    return idx >= 0;
}

bool    Poll::remove_read_fd(int fd)
{
    int idx = this->_get_or_add_fd(fd);
    if (idx >= 0)
    {
        _lst_fds[idx].events = (_lst_fds[idx].events & ~(POLLIN));
    }
    return idx >= 0;
}

bool    Poll::remove_write_fd(int fd)
{
    int idx = this->_get_or_add_fd(fd);
    if (idx >= 0)
    {
        _lst_fds[idx].events = (_lst_fds[idx].events & ~(POLLOUT));
    }
    return idx >= 0;
}

void    Poll::set_timeout(int milliseconds)
{
    _timeout_milliseconds = milliseconds;
}

void    Poll::wait_stop()
{
    _running = false;
    std::lock_guard lock(_run_mutex);
}

bool    Poll::stop()
{
    _running = false;
    return true;
}

bool    Poll::run()
{
    if (_running)
        return false;
    if (_timeout_milliseconds < 0)
    {
        LOG(error, "Poll: cannot run polling without a timeout");
        return false;
    }
    std::lock_guard lock(_run_mutex);
    _running = true;
    int ret = 0;
    while (ret >= 0 && _running)
        ret = this->poll(_timeout_milliseconds);
    return ret >= 0;
}

int     Poll::poll(int milliseconds_timeout)
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
    time_t spent = _clock.now() - before;
    _last_poll_time = spent;
    this->_process(ret);
    return ret;
}

void    Poll::_process(int poll_return)
{
    _lst_events.clear();
    _timedout = poll_return == 0;
    _error = poll_return < 0;
    if (poll_return < 0)
        LOG(error, "Poll: " << strerror(errno));
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
                evt.closed = revt & POLLHUP;
                evt.error = revt & (POLLNVAL | POLLERR);
                evt.readable = revt & (POLLIN | POLLPRI);
                evt.writable = revt & POLLOUT;
                --poll_return;
                _lst_events.push_back(evt);
            }
            ++i;
        }
    }
    this->notify_observers(this);
}

}