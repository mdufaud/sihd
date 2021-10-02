#include <sihd/util/Poll.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

// sterror errno
#include <string.h>
#include <errno.h>

// getrlimit
#include <sys/time.h>
#include <sys/resource.h>

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
    this->set_max_fds(limit);
}

Poll::~Poll()
{
    this->stop();
    std::lock_guard lock(_run_mutex);
    if (_timeout_handler_ptr != nullptr)
        delete _timeout_handler_ptr;
    if (_read_handler_ptr != nullptr)
        delete _read_handler_ptr;
    if (_write_handler_ptr != nullptr)
        delete _write_handler_ptr;
}

void    Poll::_init()
{
    _running = false;
    _timeout_handler_ptr = nullptr;
    _read_handler_ptr = nullptr;
    _write_handler_ptr = nullptr;
    _last_poll_time = 0;
    _timeout_milliseconds = -1; // infinite block
    _max_fds = 0;
}

int     Poll::_set_or_add_fd(int fd, short evt)
{
    if (fd < 0 || (size_t)fd >= _lst_fds.size())
        return -1;
    int idx = -1;
    size_t i = 0;
    while (i < _lst_fds.size())
    {
        // take existing fd
        if ( _lst_fds[i].fd == fd)
        {
            idx = i;
            break ;
        }
        // take first free fd
        if (idx < 0 && _lst_fds[i].fd == -1)
            idx = i;
        ++i;
    }
    if (idx < 0 && i < _max_fds)
    {
        // add new entry
        _lst_fds.push_back({fd, evt, 0});
        idx = i;
    }
    else if (idx >= 0)
    {
        // set entry
        _lst_fds[idx].fd = fd;
        _lst_fds[idx].events = evt;
    }
    return idx;
}

int    Poll::_get_fd_index(int fd)
{
    if (fd < 0 || (size_t)fd >= _lst_fds.size())
        return -1;
    size_t i = 0;
    while (i < _lst_fds.size())
    {
        if (_lst_fds[i].fd == fd)
            return i;
        ++i;
    }
    return -1;
}

bool    Poll::set_max_fds(int limit)
{
    if (limit < 0)
    {
        struct rlimit r;
        if (getrlimit(RLIMIT_NOFILE, &r) == -1)
        {
            LOG(error, "Poll: " << strerror(errno));
            return false;
        }
        _max_fds = r.rlim_cur;
    }
    else
        _max_fds = limit;
    LOG(debug, "Poll: setting file descriptors limit to " << _max_fds);
    _lst_fds.resize(_max_fds);
    rlim_t i = 0;
    while (i < _max_fds)
    {
        _lst_fds[i].events = 0;
        _lst_fds[i].revents = 0;
        _lst_fds[i].fd = -1;
        ++i;
    }
    return true;
}

bool    Poll::clear_fd(int fd)
{
    int idx = this->_get_fd_index(fd);
    if (idx >= 0)
    {
        _lst_fds[idx].events = 0;
        _lst_fds[idx].revents = 0;
        _lst_fds[idx].fd = -1;
    }
    return idx >= 0;
}

bool    Poll::set_read_fd(int fd)
{
    return this->_set_or_add_fd(fd, POLLIN);
}

bool    Poll::set_write_fd(int fd)
{
    return this->_set_or_add_fd(fd, POLLOUT);
}

void    Poll::set_timeout(int milliseconds)
{
    _timeout_milliseconds = milliseconds;
}

void    Poll::set_handlers(IHandler<int> *read_handler,
                            IHandler<int> *write_handler,
                            IHandler<int> *timeout_handler)
{
    this->set_read_handler(read_handler);
    this->set_write_handler(write_handler);
    this->set_timeout_handler(timeout_handler);
}

void    Poll::stop()
{
    _running = false;
}

bool    Poll::run()
{
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
    _running = false;
    return ret >= 0;
}

int     Poll::poll(int milliseconds_timeout)
{
    time_t before = _clock.now();
    int ret = ::poll(_lst_fds.data(), _lst_fds.size(), milliseconds_timeout);
    time_t spent = _clock.now() - before;
    this->_process(ret, spent);
    return ret;
}

void    Poll::_process(int poll_return, time_t nano_timespent)
{
    if (poll_return == 0 && _timeout_handler_ptr != nullptr)
        _timeout_handler_ptr->handle(time::to_milli(nano_timespent));
    else if (poll_return < 0)
        LOG(error, "Poll: " << strerror(errno));
    size_t i = 0;
    while (poll_return > 0 && i < _lst_fds.size())
    {
        short revt = _lst_fds[i].revents;
        int fd = _lst_fds[i].fd;
        if (revt != 0)
        {
            if (revt == POLLIN)
            {
                if (_read_handler_ptr != nullptr)
                    _read_handler_ptr->handle(fd);
                --poll_return;
            }
            else if (revt == POLLOUT)
            {
                if (_write_handler_ptr != nullptr)
                    _write_handler_ptr->handle(fd);
                --poll_return;
            }
        }
        ++i;
    }
}

}