#ifndef __SIHD_UTIL_POLL_HPP__
#define __SIHD_UTIL_POLL_HPP__

#include <atomic>
#include <mutex>
#include <vector>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/Clocks.hpp>
#include <sihd/util/Observable.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/platform.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <poll.h>         //pollfd
# include <sys/resource.h> //rlim_t
#else
# include <winsock2.h>
#endif

namespace sihd::util
{

struct PollEvent
{
        int fd = -1;
        bool readable = false;
        bool writable = false;
        bool error = false;
        bool closed = false;
};

class Poll: public Observable<Poll>,
            public ABlockingService
{
    public:
        Poll();
        Poll(int limit);
        ~Poll();

        // can size the poll array in advance
        void resize(int nfds);
        // if limit is negative, look for the soft curr rlimit for RLIMIT_NOFILE
        bool set_limit(int limit);

        void clear_fds();
        bool clear_fd(int fd);
        bool set_read_fd(int fd);
        bool set_write_fd(int fd);
        bool rm_read_fd(int fd);
        bool rm_write_fd(int fd);
        bool set_timeout(int milliseconds);

        // returns number of polleable fds
        int poll(int milliseconds_timeout = -1);

        // filled with file descriptors - call in observer
        const std::vector<PollEvent> & events() const { return _lst_events; };

        time_t polling_time() const { return _last_poll_time; }
        bool polling_timeout() const { return _timedout; }
        bool polling_error() const { return _error; }

        size_t read_fds_size() const;
        size_t write_fds_size() const;
        size_t fds_size() const { return _lst_fds.size(); }
        rlim_t max_fds() const { return _max_fds; };
        // in ms
        time_t timeout() const { return _timeout_milliseconds; }

    protected:
        bool on_start() override;
        bool on_stop() override;

        void process_poll_results(int poll_return);

    private:
        bool _running;
        int _timeout_milliseconds;

        mutable std::mutex _fds_mutex;
        rlim_t _max_fds;
        std::vector<struct pollfd> _lst_fds;
        SteadyClock _clock;

        std::vector<PollEvent> _lst_events;
        Timestamp _last_poll_time;
        bool _timedout;
        bool _error;
};

} // namespace sihd::util

#endif
