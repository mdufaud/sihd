#ifndef __SIHD_UTIL_PROCESS_HPP__
#define __SIHD_UTIL_PROCESS_HPP__

#include <atomic>
#include <csignal>
#include <functional>

#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Poll.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/traits.hpp>

namespace sihd::util
{

class Process: public IHandler<Poll *>,
               public ABlockingService
{
    public:
#if !defined(__SIHD_WINDOWS__)
        using FileDescType = int;
        using ReturnCodeType = uint8_t;
#else
        using FileDescType = HANDLE;
        using ReturnCodeType = DWORD;
#endif

        Process();
        Process(std::function<int()> fun);
        Process(const std::vector<std::string> & args);
        Process(std::initializer_list<std::string> args);
        Process(std::initializer_list<const char *> args);

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
        bool wait(int options = 0);
        // wait for process but do not hang
        bool wait_no_hang();

        bool can_read_pipes() const;
        // read pipes to call the callbacks on pipes
        bool read_pipes(int milliseconds_timeout = 1);
        /**
         * Read remaining pipes
         * Close the file descriptors to end processes input/output
         * Tries and wait for the process to finish, then kills it if it does not
         * Call this when piped processes (calls wait())
         */
        bool terminate();
        // send signal to process - SIGTERM by default
        bool kill(int sig = -1);

        /**
         * Setup pipes
         */
        Process & stdin_close();
        Process & stdin_from(const std::string & input);
        bool stdin_from_file(std::string_view path);
        Process & stdin_from(FileDescType fd);
        // Close stdin pipe after execution so that the process do not hang
        Process & stdin_close_after_exec(bool activate);

        Process & stdout_close();
        Process & stdout_to(std::function<void(std::string_view)> fun);
        Process & stdout_to(std::string & output);
        Process & stdout_to(Process & proc);
        Process & stdout_to(FileDescType fd);
        bool stdout_to_file(std::string_view path, bool append = false);

        Process & stderr_close();
        Process & stderr_to(std::function<void(std::string_view)> fun);
        Process & stderr_to(std::string & output);
        Process & stderr_to(Process & proc);
        Process & stderr_to(FileDescType fd);
        bool stderr_to_file(std::string_view path, bool append = false);

        void clear_argv();
        Process & add_argv(const std::string & arg);
        Process & add_argv(const std::vector<std::string> & args);
        Process & set_function(std::nullptr_t);
        Process & set_function(std::function<int()> fun);

        void env_clear();
        void env_load(const char **to_load_environ);
        void env_load(const std::vector<std::string> & to_load_environ);
        void env_set(std::string_view key, std::string_view value);
        bool env_rm(std::string_view key);
        std::optional<std::string> env_get(std::string_view key) const;

        void set_chdir(std::string_view path);

#if !defined(__SIHD_WINDOWS__)
        // LINUX only methods

        // need admin rights and work only with forks
        void set_chroot(std::string_view path);

        // use fork instead of spawn
        void set_force_fork(bool active);

        // wait for process - report dead child
        bool wait_exit(int options = 0);
        // wait for process - report stopped child
        bool wait_stop(int options = 0);
        // wait for process - report continued child
        bool wait_continue(int options = 0);
        // wait for process
        bool wait_any(int options = 0);

        bool has_exited() const;
        bool has_core_dumped() const;
        bool has_stopped_by_signal() const;
        bool has_exited_by_signal() const;
        bool has_continued() const;
        uint8_t signal_exit_number() const;
        uint8_t signal_stop_number() const;

        // get running process id
        pid_t pid() const;
#else
        // get running process id
        DWORD pid() const;
        // get running process handle
        HANDLE process() const;
#endif

        ReturnCodeType return_code() const;
        bool has_terminated() const;

        // get setted argv
        const std::vector<std::string> & argv() const { return _argv; }
        const std::vector<std::string> & env() const { return _environment; }

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
        struct Impl;

        void handle(Poll *poll) override;

        bool _do_execute(const std::vector<const char *> & argv, const std::vector<const char *> & env);
        bool _do_fork(const std::vector<const char *> & argv, const std::vector<const char *> & env);
        bool _do_spawn(const std::vector<const char *> & argv, const std::vector<const char *> & env);
        bool _do_child_process(const std::vector<const char *> & argv, const std::vector<const char *> & env);

        std::atomic<bool> _started;
        std::atomic<bool> _executing;
        bool _close_stdin_after_exec;
        std::unique_ptr<Impl> _impl;
        std::vector<std::string> _argv;
        std::vector<std::string> _environment;
        std::string _chroot;
        std::string _chdir;
        bool _force_fork;
        std::function<int()> _fun_to_execute;
        Waitable _waitable;
        Poll _poll;
};

} // namespace sihd::util

#endif