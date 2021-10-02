#ifndef __SIHD_UTIL_POLL_HPP__
# define __SIHD_UTIL_POLL_HPP__

# include <sys/resource.h> //rlim_t
# include <poll.h>
# include <vector>
# include <mutex>
# include <sihd/util/Observable.hpp>
# include <sihd/util/IRunnable.hpp>
# include <sihd/util/IHandler.hpp>
# include <sihd/util/Clocks.hpp>

namespace sihd::util
{

class Poll: virtual public IRunnable
{
    public:
        Poll();
        Poll(int limit);
        virtual ~Poll();

        // if limit is negative, look for the soft curr rlimit for RLIMIT_NOFILE
        bool set_max_fds(int limit);
        int max_fds() { return _max_fds; };

        void set_handlers(IHandler<int> *read_handler, IHandler<int> *write_handler, IHandler<int> *timeout_handler);
        void set_read_handler(IHandler<int> *handler) { _read_handler_ptr = handler; };
        void set_write_handler(IHandler<int> *handler) { _write_handler_ptr = handler; };
        void set_timeout_handler(IHandler<int> *handler) { _timeout_handler_ptr = handler; };
        bool clear_fd(int fd);
        bool set_read_fd(int fd);
        bool set_write_fd(int fd);
        void set_timeout(int milliseconds);

        bool run();
        void stop();

        int poll(int milliseconds_timeout = -1);

    protected:
    
    private:
        int _get_fd_index(int fd);
        int _set_or_add_fd(int fd, short ev);
        void _init();
        void _process(int poll_return, time_t nano_timespent);

        std::mutex _run_mutex;
        int _timeout_milliseconds;
        IHandler<int> *_read_handler_ptr;
        IHandler<int> *_write_handler_ptr;
        IHandler<int> *_timeout_handler_ptr;
        bool _running;
        time_t _last_poll_time;
        rlim_t _max_fds;
        std::vector<struct pollfd> _lst_fds;
        SystemClock _clock;
};

}

#endif 