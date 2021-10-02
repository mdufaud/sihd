#ifndef __SIHD_UTIL_PROCESS_HPP__
# define __SIHD_UTIL_PROCESS_HPP__

# include <sihd/util/Str.hpp>
# include <sihd/util/IRunnable.hpp>
# include <sihd/util/platform.hpp>
# include <functional>
# include <optional>
# include <signal.h>

# if !defined(__SIHD_WINDOWS__)
#  include <spawn.h>
# endif

namespace sihd::util
{

# if !defined(__SIHD_WINDOWS__)

class Process: virtual public IRunnable
{
    public:
        Process();
        Process(std::function<int()> fun);
        Process(const std::vector<std::string> & args);
        Process(std::initializer_list<const char *> args);
        virtual ~Process();

        // reset so you can run again
        void clear();
        // execute binary / function
        bool run();

        Process & stdin_from(const std::string & input);
        Process & stdin_from(int fd);
        bool stdin_from_file(const std::string & path);

        Process & stdout_to(std::function<void(const char *, ssize_t)> fun);
        Process & stdout_to(std::string & output);
        Process & stdout_to(int fd);
        Process & stdout_to(Process & proc);
        bool stdout_to_file(const std::string & path, bool append = false);

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

        std::optional<bool> has_exited();
        std::optional<bool> has_core_dumped();
        std::optional<bool> has_stopped_by_signal();
        std::optional<bool> has_exited_by_signal();
        std::optional<bool> has_continued();
        std::optional<int>  signal_exit_number();
        std::optional<int>  signal_stop_number();
        std::optional<int>  return_code();

        // wait for process to end
        std::optional<int> wait(int options = 0);
        // read pipes
        bool process();
        // you have to call end when you piped (calls wait())
        bool end();
        // check if run has been called
        bool has_run();
        // send signal to process
        bool kill(int sig = SIGTERM);

        // get running process id
        pid_t pid() { return _pid; };

        // get setted argv
        const std::vector<const char *> & argv() { return _argv; }

        // check if process will execute fork + exit(fun())
        bool runs_function() { return _fun_to_execute ? true : false; }

        // default file opening mode
        mode_t open_mode;

    private:
        struct FileDescWrapper {
            int fd_read = -1;
            int fd_write = -1;
            bool from_file = false;
            bool append_to_file = false;
            std::function<void(const char *, ssize_t)> fun;
            std::optional<std::reference_wrapper<std::string>> str_out;
        };

        // fd utilities
        std::pair<int, int> _pipe();
        void _clear(FileDescWrapper & fdw);
        void _add_pipe(FileDescWrapper & fdw);
        void _dup_close(int fd_from, int fd_to);
        void _close(int fd);

        // process fds once child process executed
        bool _read_fd(int fd, std::function<void(const char *, ssize_t)> fun);
        bool _write_into_fd(int fd, const std::string & str);
        bool _write_into_file(int fd, std::string & path, bool append = false);
        bool _process_fd_out(FileDescWrapper & fdw);

        // spawn but for a function
        void _do_fork();

        // spawn
        void _add_dup_action(posix_spawn_file_actions_t *actions, int dup_from, int dup_to);
        void _add_close_action(posix_spawn_file_actions_t *actions, int fd);
        bool _do_spawn();

        // fd redirections setting
        void _fdw_to(FileDescWrapper & fdw, std::function<void(const char *, ssize_t)> && fun);
        void _fdw_to(FileDescWrapper & fdw, std::string & output);
        void _fdw_to(FileDescWrapper & fdw, int fd);
        bool _fdw_to_file(FileDescWrapper & fdw, const std::string & path, bool append);

        pid_t _pid;
        FileDescWrapper _stdin;
        FileDescWrapper _stdout;
        FileDescWrapper _stderr;
        std::optional<int> _status;
        std::vector<const char *> _argv;
        std::function<int()> _fun_to_execute;
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