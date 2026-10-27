#ifndef __SIHD_SYS_EVENTPOLL_HPP__
#define __SIHD_SYS_EVENTPOLL_HPP__

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <sihd/sys/Poll.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Observable.hpp>

namespace sihd::sys
{

// fds change while waiting and stop() ends the wait at once
class EventPoll: public sihd::util::Observable<EventPoll>,
                 public sihd::util::ABlockingService
{
    public:
        EventPoll();
        ~EventPoll();

        EventPoll(const EventPoll &) = delete;
        EventPoll & operator=(const EventPoll &) = delete;

        // false for a fd the platform cannot watch (a regular file with epoll)
        bool set_read_fd(int fd);
        bool set_write_fd(int fd);
        bool rm_read_fd(int fd);
        bool rm_write_fd(int fd);
        bool clear_fd(int fd);
        void clear_fds();
        bool set_timeout(int milliseconds);
        // max fds watched, a new fd over it is refused - negative: no limit
        bool set_limit(int limit);

        // returns the events reported, polling_timeout() tells a timeout from a wake up - a signal resumes the wait
        int poll(int milliseconds_timeout = -1);

        // ends the loop after the current poll: safe from the polling thread, unlike stop()
        void request_stop();

        const std::vector<PollEvent> & events() const;

        sihd::util::Duration polling_time() const;
        bool polling_timeout() const;
        bool polling_error() const;

        size_t read_fds_size() const;
        size_t write_fds_size() const;
        size_t fds_size() const;

        int timeout_milliseconds() const;

    protected:
        bool on_start() override;
        bool on_stop() override;

    private:
        struct Interest
        {
                bool read = false;
                bool write = false;
        };

        struct Impl;

        // the interest table is locked
        bool _apply_interest(int fd, bool read, bool write);
        bool _watch(int fd, Interest interest, bool watched);
        void _unwatch(int fd);
        void _wake();
        // fills every readiness reported but the wake up, the caller drops what the table no longer wants
        int _wait(int milliseconds_timeout, std::vector<PollEvent> & ready);

        std::unique_ptr<Impl> _impl;
        mutable std::mutex _interests_mutex;
        std::map<int, Interest> _interests;
        std::atomic<bool> _stop = false;
        int _timeout_milliseconds = -1;
        std::atomic<int> _limit = -1;
        sihd::util::SteadyClock _clock;
        std::vector<PollEvent> _events;
        sihd::util::Duration _last_poll_time;
        std::atomic<bool> _timedout = false;
        std::atomic<bool> _error = false;
        int _poll_errno = 0;
};

} // namespace sihd::sys

#endif
