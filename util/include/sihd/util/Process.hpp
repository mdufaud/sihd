#ifndef __SIHD_UTIL_PROCESS_HPP__
# define __SIHD_UTIL_PROCESS_HPP__

# include <sihd/util/platform.hpp>
# include <sihd/util/Str.hpp>
# include <sihd/util/IStoppableRunnable.hpp>
# include <sihd/util/Waitable.hpp>
# include <sihd/util/Poll.hpp>
# include <sihd/util/Handler.hpp>
# include <functional>
# include <optional>
# include <signal.h>

# if !defined(__SIHD_WINDOWS__) && !defined(__ANDROID__)
#  include <spawn.h>
# endif

namespace sihd::util
{

# if !defined(__SIHD_WINDOWS__)

class Process: public IStoppableRunnable, public IHandler<Poll *>
{
    public:
        Process();
        Process(std::function<int()> fun);
        Process(const std::vector<std::string> & args);
        Process(std::initializer_list<const char *> args);
        virtual ~Process();

        // reset so you can run again
        void reset();
        // reset and clear file descriptors
        void clear();
        // execute binary / function
        bool start();

        // start process then if stdout or stderr is piped, polls them until stopped
        bool run();
        bool is_running() const;
        bool stop();
        bool wait_process_end(time_t nano_duration = 0);

        // wait for process
        bool wait(int options);
        // wait for process
        bool wait_exit(int options = 0);
        // wait for process
        bool wait_stop(int options = 0);
        // wait for process
        bool wait_continue(int options = 0);
        // wait for process
        bool wait_any(int options = 0);
        // read pipes
        bool read_pipes();
        // you have to call end when you piped (calls wait())
        bool end();
        // send signal to process
        bool kill(int sig = SIGTERM);

        Process & stdin_close();
        Process & stdin_from(const std::string & input);
        Process & stdin_from(int fd);
        bool stdin_from_file(const std::string & path);

        Process & stdout_close();
        Process & stdout_to(std::function<void(const char *, ssize_t)> fun);
        Process & stdout_to(std::string & output);
        Process & stdout_to(int fd);
        Process & stdout_to(Process & proc);
        bool stdout_to_file(const std::string & path, bool append = false);

        Process & stderr_close();
        Process & stderr_to(std::function<void(const char *, ssize_t)> fun);
        Process & stderr_to(std::string & output);
        Process & stderr_to(int fd);
        Process & stderr_to(Process & proc);
        bool stderr_to_file(const std::string & path, bool append = false);

        void clear_argv();
        Process & add_argv(const std::string & arg);
        Process & add_argv(const std::vector<std::string> & args);
        Process & set_function(std::nullptr_t);
        Process & set_function(std::function<int()> fun);

        bool has_exited() const;
        bool has_core_dumped() const;
        bool has_stopped_by_signal() const;
        bool has_exited_by_signal() const;
        bool has_continued() const;
        int signal_exit_number() const;
        int signal_stop_number() const;
        int return_code() const;

        // const std::optional<int> & status() const { return _status; }

        // get running process id
        pid_t pid() const { return _pid; };

        // get setted argv
        const std::vector<const char *> & argv() const { return _argv; }

        // check if process will execute fork + exit(fun())
        bool runs_function() const { return _fun_to_execute ? true : false; }

        // default file opening mode
        mode_t open_mode;

    private:
        enum FileDescAction
        {
            NONE,
            FILE,
            FILE_APPEND,
            CLOSE,
        };

        struct FileDescWrapper {
            int fd_read = -1;
            int fd_write = -1;
            FileDescAction action = NONE;
            std::function<void(const char *, ssize_t)> fun;
            //std::optional<std::reference_wrapper<std::string>> str_out;
            std::string path;
        };

        // fd utilities
        std::pair<int, int> _pipe();
        void _clear_fdw(FileDescWrapper & fdw);
        void _add_pipe(FileDescWrapper & fdw);
        void _dup_close(int fd_from, int fd_to);
        void _close(int & fd);

        // process fds once child process executed
        bool _read_fd(int fd, std::function<void(const char *, ssize_t)> fun);
        bool _write_into_fd(int fd, const std::string & str);
        bool _write_into_file(int fd, std::string & path, bool append = false);
        bool _process_fd_out(FileDescWrapper & fdw);

        // spawn but for a function
        void _do_fork();
        int _exec_child();

#if !defined(__SIHD_ANDROID__)
        // spawn
        void _add_dup_action(posix_spawn_file_actions_t *actions, int dup_from, int dup_to);
        void _add_close_action(posix_spawn_file_actions_t *actions, int fd);
        bool _do_spawn();
#endif
        void _init();
        void _init_poll();
        // fd redirections setting
        void _fdw_close(FileDescWrapper & fdw);
        void _fdw_to(FileDescWrapper & fdw, std::function<void(const char *, ssize_t)> && fun);
        void _fdw_to(FileDescWrapper & fdw, std::string & output);
        void _fdw_to(FileDescWrapper & fdw, int fd);
        bool _fdw_to_file(FileDescWrapper & fdw, const std::string & path, bool append);

        void handle(Poll *poll);

        pid_t _pid;
        FileDescWrapper _stdin;
        FileDescWrapper _stdout;
        FileDescWrapper _stderr;
        std::vector<const char *> _argv;
        std::function<int()> _fun_to_execute;
        std::mutex _mutex;
        Waitable _waitable;
        Poll _poll;
        siginfo_t _info;
};

# else

class Process
{
    public:
        Process() {};
        virtual ~Process() {};
};

# endif

}

#endif 