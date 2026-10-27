#include <algorithm>
#include <ranges>

#include <sihd/sys/EventPoll.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

bool EventPoll::_apply_interest(int fd, bool read, bool write)
{
    if (fd < 0)
        return false;
    const auto it = _interests.find(fd);
    if (!read && !write)
    {
        if (it == _interests.end())
            return false;
        _interests.erase(it);
        this->_unwatch(fd);
        return true;
    }
    if (it == _interests.end() && _limit >= 0 && _interests.size() >= (size_t)_limit)
        return false;
    const Interest interest {read, write};
    if (!this->_watch(fd, interest, it != _interests.end()))
        return false;
    _interests[fd] = interest;
    return true;
}

bool EventPoll::set_read_fd(int fd)
{
    std::lock_guard lock(_interests_mutex);
    const auto it = _interests.find(fd);
    return this->_apply_interest(fd, true, it != _interests.end() && it->second.write);
}

bool EventPoll::set_write_fd(int fd)
{
    std::lock_guard lock(_interests_mutex);
    const auto it = _interests.find(fd);
    return this->_apply_interest(fd, it != _interests.end() && it->second.read, true);
}

bool EventPoll::rm_read_fd(int fd)
{
    std::lock_guard lock(_interests_mutex);
    const auto it = _interests.find(fd);
    if (it == _interests.end())
        return false;
    return this->_apply_interest(fd, false, it->second.write);
}

bool EventPoll::rm_write_fd(int fd)
{
    std::lock_guard lock(_interests_mutex);
    const auto it = _interests.find(fd);
    if (it == _interests.end())
        return false;
    return this->_apply_interest(fd, it->second.read, false);
}

bool EventPoll::clear_fd(int fd)
{
    std::lock_guard lock(_interests_mutex);
    return this->_apply_interest(fd, false, false);
}

void EventPoll::clear_fds()
{
    std::lock_guard lock(_interests_mutex);
    for (const auto & [fd, interest] : _interests)
        this->_unwatch(fd);
    _interests.clear();
}

const std::vector<PollEvent> & EventPoll::events() const
{
    return _events;
}

Duration EventPoll::polling_time() const
{
    return _last_poll_time;
}

bool EventPoll::polling_timeout() const
{
    return _timedout;
}

bool EventPoll::polling_error() const
{
    return _error;
}

int EventPoll::timeout_milliseconds() const
{
    return _timeout_milliseconds;
}

bool EventPoll::set_timeout(int milliseconds)
{
    if (milliseconds < 0)
        return false;
    _timeout_milliseconds = milliseconds;
    return true;
}

bool EventPoll::set_limit(int limit)
{
    _limit = limit;
    return true;
}

size_t EventPoll::read_fds_size() const
{
    std::lock_guard lock(_interests_mutex);
    return std::ranges::count_if(_interests, [](const auto & entry) { return entry.second.read; });
}

size_t EventPoll::write_fds_size() const
{
    std::lock_guard lock(_interests_mutex);
    return std::ranges::count_if(_interests, [](const auto & entry) { return entry.second.write; });
}

size_t EventPoll::fds_size() const
{
    std::lock_guard lock(_interests_mutex);
    return _interests.size();
}

bool EventPoll::on_stop()
{
    this->request_stop();
    return true;
}

void EventPoll::request_stop()
{
    _stop = true;
    this->_wake();
}

bool EventPoll::on_start()
{
    if (_timeout_milliseconds < 0)
    {
        SIHD_LOG(error, "EventPoll: cannot start polling without a timeout");
        return false;
    }

    // a stop landing after set_ready must not be erased here
    _stop = false;

    this->service_set_ready();
    int ret = 0;
    while (_stop == false && ret >= 0)
        ret = this->poll(_timeout_milliseconds);
    if (ret < 0)
        SIHD_LOG(error, "EventPoll: {}", os::error_str(_poll_errno));
    return ret >= 0;
}

int EventPoll::poll(int milliseconds_timeout)
{
    const Timestamp before = _clock.now();
    const time::UnixTime deadline = before + time::ms(std::max(milliseconds_timeout, 0));
    std::vector<PollEvent> ready;

    int ret = this->_wait(milliseconds_timeout, ready);
    int saved_errno = os::last_socket_error();
    while (ret < 0 && error_errno(saved_errno) == ErrorCode::interrupted)
    {
        // rounded up: a truncated remainder would time out before the deadline
        const time::UnixTime remaining = std::max<time::UnixTime>(deadline - _clock.now().nanoseconds(), 0);
        ret = this->_wait(milliseconds_timeout < 0 ? -1 : (int)time::to_ms(remaining + time::ms(1) - 1), ready);
        saved_errno = os::last_socket_error();
    }

    {
        std::lock_guard lock(_interests_mutex);
        _last_poll_time = _clock.now() - before;
        _events.clear();
        _timedout = ret == 0;
        _error = ret < 0;
        _poll_errno = saved_errno;

        for (PollEvent & event : ready)
        {
            // an event of a fd removed while waiting is stale
            const auto it = _interests.find(event.fd);
            if (it == _interests.end())
                continue;
            event.readable = event.readable && it->second.read;
            event.writable = event.writable && it->second.write;
            if (event.readable || event.writable || event.error || event.closed)
                _events.push_back(event);
        }
    }

    this->notify_observers(this);
    return ret < 0 ? ret : (int)_events.size();
}

} // namespace sihd::sys
