#ifndef __SIHD_UTIL_PROCESS_HPP__
#define __SIHD_UTIL_PROCESS_HPP__

#include <atomic>
#include <csignal>
#include <functional>

#include <sihd/util/BlockingService.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/traits.hpp>

namespace sihd::util
{

#if !defined(__SIHD_WINDOWS__)

class Process: public IHandler<Poll *>,
               public BlockingService
{
    public:
        Process();
        Process(std::function<int()> fun);
        Process(const std::vector<std::string> & args);
        Process(std::initializer_list<std::string> args);
        Process(std::initializer_list<const char *> args);

        Process(const Process &) = delete;
        Process(Process &&) = delete;

        template <typename... Args,
                  typename = typename std::enable_if_t<traits::are_all_constructible<std::string, Args...>::value>>
        Process(const Args &...args): Process()
        {
            _argv.reserve(sizeof...(Args));

            const auto filler = [this](const auto & arg) {
                _argv.emplace_back(arg);
            };

            (filler(args), ...);
        }

        ~Process();

        // reset so you can run again
        void reset_proc();
        // reset and clear file descriptors configurations
        void clear();
        // execute binary / function
        bool execute();

        bool is_process_running() const;

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
        // read pipes to call the callbacks on pipes
        bool read_pipes(int milliseconds_timeout = 1);
        /**
         * Read remaining pipes
         * Close the file descriptors to end processes input/output
         * Tries and wait for the process to finish, then kills it if it does not
         * Call this when piped processes (calls wait())
         */
        bool end();
        // send signal to process - SIGTERM by default
        bool kill(int sig = -1);

        Process & stdin_close();
        Process & stdin_from(const std::string & input);
        Process & stdin_from(int fd);
        bool stdin_from_file(std::string_view path);

        Process & stdout_close();
        Process & stdout_to(std::function<void(std::string_view)> fun);
        Process & stdout_to(std::string & output);
        Process & stdout_to(int fd);
        Process & stdout_to(Process & proc);
        bool stdout_to_file(std::string_view path, bool append = false);

        Process & stderr_close();
        Process & stderr_to(std::function<void(std::string_view)> fun);
        Process & stderr_to(std::string & output);
        Process & stderr_to(int fd);
        Process & stderr_to(Process & proc);
        bool stderr_to_file(std::string_view path, bool append = false);

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

        // get running process id
        pid_t pid() const { return _pid; };

        // get setted argv
        const std::vector<std::string> & argv() const { return _argv; }

        // check if process will execute fork + exit(fun())
        bool runs_function() const { return _fun_to_execute ? true : false; }

        // default file opening mode
        mode_t open_mode;

    protected:
        // start process then if stdout or stderr is piped, polls them until the process is stopped
        bool on_start() override;
        bool on_stop() override;
        bool wait_process_end(Timestamp nano_duration = 0);

    private:
        enum FileDescAction
        {
            NONE,
            FILE,
            FILE_APPEND,
            CLOSE,
        };

        struct FileDescWrapper
        {
                int fd_read = -1;
                int fd_write = -1;
                FileDescAction action = NONE;
                std::function<void(std::string_view)> fun;
                std::string path;
        };

        // fd utilities
        void _add_pipe(FileDescWrapper & fdw);

        // process fds once child process executed
        bool _process_fd_out(FileDescWrapper & fdw);

        // spawn but for a function
        bool _do_fork(const std::vector<const char *> & argv);

# if !defined(__SIHD_ANDROID__)
        bool _do_spawn(const std::vector<const char *> & argv);
# endif
        // fd redirections setting
        void _fdw_close(FileDescWrapper & fdw);
        void _fdw_to(FileDescWrapper & fdw, std::function<void(std::string_view)> && fun);
        void _fdw_to(FileDescWrapper & fdw, std::string & output);
        void _fdw_to(FileDescWrapper & fdw, int fd);
        bool _fdw_to_file(FileDescWrapper & fdw, std::string_view path, bool append);

        void handle(Poll *poll) override;

        std::atomic<bool> _started;
        std::atomic<bool> _executing;
        pid_t _pid;
        FileDescWrapper _stdin;
        FileDescWrapper _stdout;
        FileDescWrapper _stderr;
        std::vector<std::string> _argv;
        std::function<int()> _fun_to_execute;
        mutable std::mutex _mutex_info;
        Waitable _waitable;
        Waitable _waitable_start;
        Poll _poll;
        siginfo_t _info;
};

#else

class Process
{
    public:
        Process() = default;
        ~Process() = default;
};

#endif

} // namespace sihd::util

#endif