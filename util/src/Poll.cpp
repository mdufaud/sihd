#include <sihd/util/Poll.hpp>
#include <sihd/util/Logger.hpp>

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
    this->set_max_fds(limit);
}

Poll::~Poll()
{
    this->stop();
    std::lock_guard lock(_run_mutex);
    if (_prepoll_runnable_ptr != nullptr)
        delete _prepoll_runnable_ptr;
    if (_postpoll_handler_ptr != nullptr)
        delete _postpoll_handler_ptr;
    if (_read_handler_ptr != nullptr)
        delete _read_handler_ptr;
    if (_write_handler_ptr != nullptr)
        delete _write_handler_ptr;
}

void    Poll::_init()
{
    _running = false;
    _prepoll_runnable_ptr = nullptr;
    _postpoll_handler_ptr = nullptr;
    _read_handler_ptr = nullptr;
    _write_handler_ptr = nullptr;
    _timeout_milliseconds = -1; // infinite block
    _max_fds = 0;
}

int     Poll::_set_or_add_fd(int fd, short evt)
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
        p.fd = fd;
        p.events = evt;
        p.revents = 0;
        _lst_fds.push_back(p);
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
    int i = 0;
    while (i < nfds)
    {
        _lst_fds[i].events = 0;
        _lst_fds[i].revents = 0;
        _lst_fds[i].fd = -1;
        ++i;
    }
}

bool    Poll::set_max_fds(int limit)
{
    if (limit < 0)
    {
#if !defined(__SIHD_WINDOWS__)
        struct rlimit r;
        if (getrlimit(RLIMIT_NOFILE, &r) == -1)
        {
            LOG(error, "Poll: " << strerror(errno));
            return false;
        }
        _max_fds = r.rlim_cur;
# else
        _max_fds = 512;
# endif
    }
    else
        _max_fds = limit;
    LOG(debug, "Poll: setting file descriptors limit to " << _max_fds);
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

Poll &  Poll::set_read_handler(IHandler<int> *handler)
{
    std::lock_guard lock(_handlers_mutex);
    if (_read_handler_ptr != nullptr)
        delete _read_handler_ptr;
    _read_handler_ptr = handler;
    return *this;
}

Poll &  Poll::set_write_handler(IHandler<int> *handler)
{
    std::lock_guard lock(_handlers_mutex);
    if (_write_handler_ptr != nullptr)
        delete _write_handler_ptr;
    _write_handler_ptr = handler;
    return *this;
}

Poll &  Poll::set_prepoll_runnable(IRunnable *runnable)
{
    std::lock_guard lock(_handlers_mutex);
    if (_prepoll_runnable_ptr != nullptr)
        delete _prepoll_runnable_ptr;
    _prepoll_runnable_ptr = runnable;
    return *this;
}

Poll &  Poll::set_postpoll_handler(IHandler<time_t, bool> *handler)
{
    std::lock_guard lock(_handlers_mutex);
    if (_postpoll_handler_ptr != nullptr)
        delete _postpoll_handler_ptr;
    _postpoll_handler_ptr = handler;
    return *this;
}

bool    Poll::stop()
{
    if (_running)
    {
        _running = false;
        std::lock_guard lock(_run_mutex);
        return true;
    }
    return false;
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
    if (_prepoll_runnable_ptr != nullptr && _prepoll_runnable_ptr->run() == false)
        return -2;
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
    this->_process(ret, spent);
    return ret;
}

void    Poll::_process(int poll_return, time_t nano_timespent)
{
    std::lock_guard lock_handler(_handlers_mutex);
    if (poll_return < 0)
    {
        LOG(error, "Poll: " << strerror(errno));
        return ;
    }
    else if (_postpoll_handler_ptr != nullptr)
        _postpoll_handler_ptr->handle(nano_timespent, poll_return == 0);
    if (poll_return <= 0)
        return ;
    size_t i = 0;
    std::lock_guard lock_fds(_fds_mutex);
    while (i < _lst_fds.size())
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