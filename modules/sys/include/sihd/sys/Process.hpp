#ifndef __SIHD_SYS_PROCESS_HPP__
#define __SIHD_SYS_PROCESS_HPP__

#include <atomic>
#include <csignal>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <sihd/sys/Environment.hpp>
#include <sihd/sys/Poll.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/sys/sched.hpp>
#include <sihd/util/ABlockingService.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/traits.hpp>

namespace sihd::sys
{

class Process: public sihd::util::IHandler<Poll *>,
               public sihd::util::ABlockingService
{
    public:
        using ReturnCodeType = uint8_t;
#if !defined(__SIHD_WINDOWS__)
        using FileDescType = int;
#else
        // HANDLE, spelled without windows.h
        using FileDescType = void *;
#endif

        static constexpr bool supported = !sihd::util::build::is_emscripten;

        // POSIX-like observable code for "launch failed or killed" (-1 wrapped to 8 bits)
        static constexpr ReturnCodeType failure_return_code = 255;

        Process();
        Process(std::function<int()> fun);
        Process(std::span<const std::string> args);
        Process(std::span<std::string_view> args);
        Process(std::span<const char *> args);
        Process(std::initializer_list<std::string_view> args);

        template <typename... Args,
                  typename =
                      typename std::enable_if_t<sihd::util::traits::are_all_constructible<std::string, Args...>::value>>
        Process(const Args &...args): Process()
        {
            _argv.reserve(sizeof...(Args));

            const auto filler = [this](const auto & arg) {
                _argv.emplace_back(arg);
            };

            (filler(args), ...);
        }

        ~Process();

        void reset_proc();
        void clear();
        sihd::util::Result<void> execute();

        bool is_process_running() const;

        // wait for process: false means the process did not terminate
        sihd::util::Result<bool> wait(int options = 0);
        sihd::util::Result<bool> wait_no_hang();

        bool can_read_pipes() const;
        bool read_pipes(int milliseconds_timeout = 1);
        /**
         * Read remaining pipes
         * Close the file descriptors to end processes input/output
         * Tries and wait for the process to finish, then kills it if it does not
         * Call this when piped processes (calls wait())
         */
        bool terminate();
        sihd::util::Result<void> kill(int sig = -1);

        /**
         * Setup pipes
         */
        Process & stdin_close();
        Process & stdin_from(const std::string & input);
        bool stdin_from_file(const std::string & path);
        Process & stdin_from(FileDescType fd);
        Process & stdin_close_after_exec(bool activate);

        Process & stdout_close();
        Process & stdout_to(std::function<void(std::string_view)> fun);
        Process & stdout_to(std::string & output);
        Process & stdout_to(Process & proc);
        Process & stdout_to(FileDescType fd);
        bool stdout_to_file(const std::string & path, bool append = false);

        Process & stderr_close();
        Process & stderr_to(std::function<void(std::string_view)> fun);
        Process & stderr_to(std::string & output);
        Process & stderr_to(Process & proc);
        Process & stderr_to(FileDescType fd);
        bool stderr_to_file(const std::string & path, bool append = false);

        void clear_argv();
        Process & add_argv(std::string_view arg);
        Process & add_argv(const std::vector<std::string> & args);
        Process & set_function(std::nullptr_t);
        Process & set_function(std::function<int()> fun);

        void env_clear();

        void env_load(std::span<std::string_view> to_load_environ);
        void env_load(std::span<const std::string> to_load_environ);
        void env_load(std::span<const char *> to_load_environ);
        void env_load(std::initializer_list<std::string_view> to_load_environ);

        void env_set(std::string_view key, std::string_view value);
        bool env_rm(std::string_view key);
        std::optional<std::string> env_get(std::string_view key) const;

        void set_chdir(std::string_view path);

        bool set_nice(int nice_value);
        bool set_scheduler(sched::Policy policy, sched::Priority priority);
        bool set_affinity(const std::vector<bool> & cpu_mask);

#if !defined(__SIHD_WINDOWS__)
        // need admin rights and work only with forks
        void set_chroot(std::string_view path);

        void set_force_fork(bool active);

        sihd::util::Result<bool> wait_stop(int options = 0);
        sihd::util::Result<bool> wait_continue(int options = 0);

        bool has_core_dumped() const;
        bool has_stopped_by_signal() const;
        bool has_exited_by_signal() const;
        bool has_continued() const;
        uint8_t signal_exit_number() const;
        uint8_t signal_stop_number() const;

        pid_t pid() const;
#else
        // DWORD / HANDLE, spelled without windows.h
        unsigned long pid() const;
        void *process() const;
#endif

        // options = timeout ms, 0 = block; false means not terminated
        sihd::util::Result<bool> wait_exit(int options = 0);
        sihd::util::Result<bool> wait_any(int options = 0);
        // windows processes always terminate by exit (no signal/stop semantics)
        bool has_exited() const;

        ReturnCodeType return_code() const;
        bool has_terminated() const;

        // get setted argv
        const std::vector<std::string> & argv() const { return _argv; }
        // "KEY=VALUE" materialization of the environment
        std::vector<std::string> env() const;
        const Environment & environment() const { return _env; }

        mode_t open_mode;

    protected:
        bool on_start() override;
        bool on_stop() override;
        bool wait_process_end(sihd::util::Duration nano_duration = {});

    private:
        struct Scheduler
        {
                sched::Policy policy;
                sched::Priority priority;
        };

        struct Impl;

        void handle(Poll *poll) override;

        sihd::util::Result<void> _do_execute(const std::vector<const char *> & argv);
        sihd::util::Result<void> _do_fork(const std::vector<const char *> & argv);
        sihd::util::Result<void> _do_spawn(const std::vector<const char *> & argv);
        sihd::util::Result<void> _do_child_process(const std::vector<const char *> & argv);

        void _apply_child_policy();

        std::atomic<bool> _started;
        std::atomic<bool> _executing;
        bool _close_stdin_after_exec;
        std::unique_ptr<Impl> _impl;
        std::vector<std::string> _argv;
        Environment _env;
        std::string _chroot;
        std::string _chdir;
        bool _force_fork;
        std::optional<int> _nice;
        std::optional<Scheduler> _scheduler;
        std::vector<bool> _affinity;
        std::function<int()> _fun_to_execute;
        sihd::util::Waitable _waitable;
        Poll _poll;
};

} // namespace sihd::sys

#endif