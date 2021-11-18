#ifndef __SIHD_UTIL_POLL_HPP__
# define __SIHD_UTIL_POLL_HPP__

# include <sihd/util/platform.hpp>
# include <sihd/util/Observable.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/IHandler.hpp>
# include <sihd/util/Clocks.hpp>
# include <sihd/util/time.hpp>
# include <vector>
# include <mutex>

# if !defined(__SIHD_WINDOWS__)
#  include <sys/resource.h> //rlim_t
#  include <poll.h> //pollfd
# else
typedef unsigned long rlim_t;
#  include <winsock2.h>
# endif

namespace sihd::util
{

class Poll: public IStoppableRunnable, public Observable<Poll>
{
    public:
        struct PollEvent
        {
            int fd = -1;
            bool readable = false;
            bool writable = false;
            bool error = false;
            bool closed = false;
        };

        Poll();
        Poll(int limit);
        virtual ~Poll();

        // can size the poll array in advance
        void resize(int nfds);
        // if limit is negative, look for the soft curr rlimit for RLIMIT_NOFILE
        bool set_limit(int limit);
        int max_fds() { return _max_fds; };

        void clear_fds();
        bool clear_fd(int fd);
        bool set_read_fd(int fd);
        bool set_write_fd(int fd);
        bool remove_write_fd(int fd);
        bool remove_read_fd(int fd);
        void set_timeout(int milliseconds);

        bool run();
        bool stop();
        void wait_stop();
        bool is_running() const { return _running; }

        int poll(int milliseconds_timeout = -1);

        const std::vector<PollEvent> & get_events() const { return _lst_events; };
        time_t polling_time() const { return _last_poll_time; }
        bool polling_timeout() const { return _timedout; }
        bool polling_error() const { return _error; }

        size_t read_fds_size();
        size_t write_fds_size();
        size_t fds_size() const { return _lst_fds.size(); }
        time_t timeout() const { return _timeout_milliseconds; }

    protected:
    
    private:
        int _get_fd_index(int fd);
        int _get_or_add_fd(int fd);
        void _init();
        void _process(int poll_return);

        std::mutex _fds_mutex;
        std::mutex _run_mutex;
        int _timeout_milliseconds;

        bool _running;
        rlim_t _max_fds;
        std::vector<struct pollfd> _lst_fds;
        SystemClock _clock;

        std::vector<PollEvent> _lst_events;
        time_t _last_poll_time;
        bool _timedout;
        bool _error;
};

}

#endif