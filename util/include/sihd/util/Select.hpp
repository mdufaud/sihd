#ifndef __SIHD_UTIL_SELECT_HPP__
# define __SIHD_UTIL_SELECT_HPP__

# include <sihd/util/platform.hpp>

# if !defined(__SIHD_WINDOWS__)
#  include <sys/select.h>
#  include <sys/resource.h> //rlim_t
#  include <sys/time.h>
# else
typedef unsigned long rlim_t;
#  include <stdint.h>
#  include <winsock2.h>
#  include <winsock.h>

# endif

# include <list>
# include <mutex>
# include <sihd/util/Observable.hpp>
# include <sihd/util/IRunnable.hpp>
# include <sihd/util/IHandler.hpp>
# include <sihd/util/Clocks.hpp>
# include <sihd/util/time.hpp>

namespace sihd::util
{

class Select
{
    public:
        Select();
        virtual ~Select();

        // if limit is negative, look for the soft curr rlimit for RLIMIT_NOFILE on linux or set to 512 in windows
        bool set_max_fds(int limit);
        int max_fds() { return _max_fds; };

        void set_handlers(IHandler<int> *read_handler, IHandler<int> *write_handler, IHandler<time_t, bool> *timeout_handler);
        void set_read_handler(IHandler<int> *handler);
        void set_write_handler(IHandler<int> *handler);
        // called after every poll if poll did not fail with nanoseconds spent in poll and 
        void set_timeout_handler(IHandler<time_t, bool> *handler);
        void clear_fds();
        bool clear_fd(int fd);
        bool set_read_fd(int fd);
        bool set_write_fd(int fd);
        void set_timeout(int milliseconds);

        bool run();
        void stop();

        int select(int milliseconds = -1);

        bool is_running() const { return _running; }
        IHandler<int> *get_read_handler() const { return _read_handler_ptr; }
        IHandler<int> *get_write_handler() const { return _write_handler_ptr; }
        IHandler<time_t, bool> *get_timeout_handler() const { return _timeout_handler_ptr; }

    protected:
    
    private:
        void _init();
        void _setup_select();
        int _do_select(int milliseconds);
        void _process_select(int select_return, time_t nano_timespent);

        std::mutex _handlers_mutex;
        std::mutex _fds_mutex;
        std::mutex _run_mutex;
        int _timeout;
        IHandler<int> *_read_handler_ptr;
        IHandler<int> *_write_handler_ptr;
        IHandler<time_t, bool> *_timeout_handler_ptr;
        bool _running;
        int _highest_fd;
        rlim_t _max_fds;
        std::list<int> _lst_read_fds;
        std::list<int> _lst_write_fds;
        fd_set _fds_read;
        fd_set _fds_write;
        SystemClock _clock;
};

}

#endif