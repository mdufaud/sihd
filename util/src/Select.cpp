#include <sihd/util/Select.hpp>
#include <sihd/util/Logger.hpp>

// sterror errno
# include <string.h>
# include <errno.h>

# include <algorithm>

namespace sihd::util
{

LOGGER;

Select::Select()
{
    this->_init();
}

Select::~Select()
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

void    Select::_init()
{
    _running = false;
    _timeout_handler_ptr = nullptr;
    _read_handler_ptr = nullptr;
    _write_handler_ptr = nullptr;
    _timeout = -1; // infinite block
    _max_fds = 0;
    _highest_fd = 0;
}

bool    Select::set_max_fds(int limit)
{
    if (limit < 0)
    {
# if !defined (__SIHD_WINDOWS__)
        struct rlimit r;
        if (getrlimit(RLIMIT_NOFILE, &r) == -1)
        {
            LOG(error, "Select: " << strerror(errno));
            return false;
        }
        _max_fds = r.rlim_cur;
# else
        _max_fds = 512;
# endif
    }
    else
        _max_fds = limit;
    LOG(debug, "Select: setting file descriptors limit to " << _max_fds);
    return true;
}

void    Select::clear_fds()
{
    std::lock_guard lock(_fds_mutex);
    _lst_read_fds.clear();
    _lst_write_fds.clear();
}

bool    Select::clear_fd(int fd)
{
    if (fd < 0 || (rlim_t)fd >= _max_fds)
        return false;
    std::lock_guard lock(_fds_mutex);
    auto it_read = std::find(_lst_read_fds.begin(), _lst_read_fds.end(), fd);
    auto it_write = std::find(_lst_write_fds.begin(), _lst_write_fds.end(), fd);
    if (it_read != _lst_read_fds.end())
        _lst_read_fds.erase(it_read);
    if (it_write != _lst_write_fds.end())
        _lst_write_fds.erase(it_write);
    return it_read != _lst_read_fds.end() || it_write != _lst_write_fds.end();
}

bool    Select::set_read_fd(int fd)
{
    if (fd < 0 || (rlim_t)fd >= _max_fds)
        return false;
    std::lock_guard lock(_fds_mutex);
    bool ret = std::find(_lst_read_fds.begin(), _lst_read_fds.end(), fd) == _lst_read_fds.end();
    if (ret)
        _lst_read_fds.push_back(fd);
    return ret;
}

bool    Select::set_write_fd(int fd)
{
    if (fd < 0 || (rlim_t)fd >= _max_fds)
        return false;
    std::lock_guard lock(_fds_mutex);
    bool ret = std::find(_lst_write_fds.begin(), _lst_write_fds.end(), fd) == _lst_write_fds.end();
    if (ret)
        _lst_write_fds.push_back(fd);
    return ret;
}

void    Select::set_timeout(int milliseconds)
{
    _timeout = milliseconds;
}

void    Select::set_handlers(IHandler<int> *read_handler,
                            IHandler<int> *write_handler,
                            IHandler<time_t, bool> *timeout_handler)
{
    this->set_read_handler(read_handler);
    this->set_write_handler(write_handler);
    this->set_timeout_handler(timeout_handler);
}

void    Select::set_read_handler(IHandler<int> *handler)
{
    std::lock_guard lock(_handlers_mutex);
    if (_read_handler_ptr != nullptr)
        delete _read_handler_ptr;
    _read_handler_ptr = handler;
}

void    Select::set_write_handler(IHandler<int> *handler)
{
    std::lock_guard lock(_handlers_mutex);
    if (_write_handler_ptr != nullptr)
        delete _write_handler_ptr;
    _write_handler_ptr = handler;
}

void    Select::set_timeout_handler(IHandler<time_t, bool> *handler)
{
    std::lock_guard lock(_handlers_mutex);
    if (_timeout_handler_ptr != nullptr)
        delete _timeout_handler_ptr;
    _timeout_handler_ptr = handler;
}

bool    Select::stop()
{
    if (_running)
    {
        _running = false;
        std::lock_guard lock(_run_mutex);
        return true;
    }
    return false;
}

bool    Select::run()
{
    if (_timeout < 0)
    {
        LOG(error, "Select: cannot run select without a timeout");
        return false;
    }
    std::lock_guard lock(_run_mutex);
    _running = true;
    int ret = 0;
    while (ret >= 0 && _running)
        ret = this->select(_timeout);
    _running = false;
    return ret >= 0;
}

void    Select::_setup_select()
{
    FD_ZERO(&_fds_read);
    FD_ZERO(&_fds_write);
    std::lock_guard lock(_fds_mutex);
    for (const int & fd: _lst_read_fds)
    {
        FD_SET(fd, &_fds_read);
        _highest_fd = _highest_fd < fd ? fd : _highest_fd;
    }
    for (const int & fd: _lst_write_fds)
    {
        FD_SET(fd, &_fds_write);
        _highest_fd = _highest_fd < fd ? fd : _highest_fd;
    }
}

void    Select::_process_select(int select_return, time_t nano_timespent)
{
    std::lock_guard lock_handlers(_handlers_mutex);
    if (select_return < 0)
    {
        LOG(error, "Select: " << strerror(errno));
        return ;
    }
    else if (_timeout_handler_ptr != nullptr)
        _timeout_handler_ptr->handle(nano_timespent, select_return == 0);
    if (select_return <= 0)
        return ;
    std::lock_guard lock_fds(_fds_mutex);
    for (const int & fd: _lst_read_fds)
    {
        if (FD_ISSET(fd, &_fds_read))
            _read_handler_ptr->handle(fd);
    }
    for (const int & fd: _lst_write_fds)
    {
        if (FD_ISSET(fd, &_fds_write))
            _write_handler_ptr->handle(fd);
    }
}

int    Select::_do_select(int milliseconds)
{
    struct timeval tv;
    struct timeval *timeout_struct = nullptr;

    if (milliseconds >= 0)
    {
        timeout_struct = &tv;
        tv = time::tv_from_milli(milliseconds);
    }
    int ret;
    {
        std::lock_guard lock(_fds_mutex);
        ret = ::select(_highest_fd + 1, &_fds_read, &_fds_write, nullptr, timeout_struct);
    }
    return ret;
}

int    Select::select(int milliseconds)
{
    time_t before = _clock.now();
    this->_setup_select();
    int ret = this->_do_select(milliseconds);
    time_t spent = _clock.now() - before;
    this->_process_select(ret, spent);
    return ret;
}

}