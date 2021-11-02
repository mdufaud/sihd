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

class Poll: public IStoppableRunnable
{
    public:
        Poll();
        Poll(int limit);
        virtual ~Poll();

        // can size the poll array in advance
        void resize(int nfds);
        // if limit is negative, look for the soft curr rlimit for RLIMIT_NOFILE
        bool set_max_fds(int limit);
        int max_fds() { return _max_fds; };

        Poll & set_read_handler(IHandler<int> *handler);
        Poll & set_write_handler(IHandler<int> *handler);
        // called after every poll if poll did not fail with nanoseconds spent in poll and bool if no activity
        Poll & set_prepoll_runnable(IRunnable *runnable);
        Poll & set_postpoll_handler(IHandler<time_t, bool> *handler);
        void clear_fds();
        bool clear_fd(int fd);
        bool set_read_fd(int fd);
        bool set_write_fd(int fd);
        void set_timeout(int milliseconds);

        bool run();
        bool stop();
        bool is_running() const { return _running; }

        int poll(int milliseconds_timeout = -1);

        IHandler<int> *get_read_handler() const { return _read_handler_ptr; }
        IHandler<int> *get_write_handler() const { return _write_handler_ptr; }
        IHandler<time_t, bool> *get_timeout_handler() const { return _postpoll_handler_ptr; }

    protected:
    
    private:
        int _get_fd_index(int fd);
        int _set_or_add_fd(int fd, short ev);
        void _init();
        void _process(int poll_return, time_t nano_timespent);

        std::mutex _handlers_mutex;
        std::mutex _fds_mutex;
        std::mutex _run_mutex;
        int _timeout_milliseconds;
        IHandler<int> *_read_handler_ptr;
        IHandler<int> *_write_handler_ptr;
        IRunnable *_prepoll_runnable_ptr;
        IHandler<time_t, bool> *_postpoll_handler_ptr;
        bool _running;
        rlim_t _max_fds;
        std::vector<struct pollfd> _lst_fds;
        SystemClock _clock;
};

}

#endif