#include <sihd/sys/platform.hpp>

#if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# define ENABLE_SPAWN
# include <spawn.h>
#endif

#if !defined(__SIHD_EMSCRIPTEN__)
# define ENABLE_FORK
#endif

#include <fcntl.h>    // open
#include <sys/stat.h> // open
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <stdexcept>

#ifndef SIHD_PROCESS_READ_BUFFER_SIZE
# define SIHD_PROCESS_READ_BUFFER_SIZE 2048
#endif

#ifndef SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE
# define SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE 0740
#endif

#include <sihd/sys/File.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/str.hpp>

extern "C"
{
extern char **environ;
}

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

/**
 * Functions helper
 */

namespace
{

void safe_close(int & fd)
{
    if (fd == -1)
        return;
    if (close(fd) == -1)
    {
        SIHD_LOG(error, "Process: could not close fd: {}", os::last_error_str());
    }
    else
    {
        fd = -1;
    }
}

# if defined(ENABLE_SPAWN)

void add_dup_action(posix_spawn_file_actions_t *actions, int dup_from, int dup_to)
{
    if (dup_from >= 0 && dup_to >= 0)
    {
        posix_spawn_file_actions_adddup2(actions, dup_from, dup_to);
        if (dup_from != dup_to)
            posix_spawn_file_actions_addclose(actions, dup_from);
    }
}

void add_close_action(posix_spawn_file_actions_t *actions, int fd)
{
    if (fd >= 0)
        posix_spawn_file_actions_addclose(actions, fd);
}

# endif // ENABLE_SPAWN

# if defined(ENABLE_FORK)

void setup_environ_in_child_process(const std::vector<const char *> & env)
{
    for (const char *keyval : env)
    {
        if (keyval == nullptr)
            continue;
        auto [key, val] = str::split_pair(keyval, "=");
        if (!key.empty())
        {
            setenv(key.c_str(), val.empty() ? "" : val.c_str(), 1);
        }
    }
}

void dup_close(int fd_from, int fd_to)
{
    if (fd_from < 0)
        return;
    if (dup2(fd_from, fd_to) == -1)
    {
        SIHD_LOG(error, "Process: could not duplicate fd: {}", os::last_error_str());
    }
    if (fd_from != fd_to)
        safe_close(fd_from);
}

# endif // ENABLE_FORK

std::pair<int, int> make_pipe()
{
    int fd[2];

    if (pipe(fd) < 0)
        throw std::runtime_error("Cannot make pipe");
    // read - write;
    return std::make_pair(fd[0], fd[1]);
}

bool read_pipe_into_callback(int fd, std::function<void(std::string_view)> fun)
{
    char buffer[SIHD_PROCESS_READ_BUFFER_SIZE + 1];
    ssize_t ret;

    ret = read(fd, &buffer, SIHD_PROCESS_READ_BUFFER_SIZE);
    if (ret > 0)
    {
        buffer[ret] = 0;
        fun(std::string_view {buffer, (size_t)ret});
    }
    return ret > 0;
}

bool write_into_pipe(int fd, const std::string & str)
{
    ssize_t ret = write(fd, str.c_str(), str.size());
    return ret >= 0 && ret == (ssize_t)str.size();
}

bool read_pipe_into_file(int fd, const std::string & path, bool append)
{
    File file(path, append ? "a" : "w");

    if (!file.is_open())
        return false;

    auto fun = [&file](std::string_view buffer) {
        file.write(buffer.data(), buffer.size());
    };
    return read_pipe_into_callback(fd, fun);
}

void init_poller(sihd::sys::Poll & poll, int stdout_fd, int stderr_fd)
{
    poll.stop();
    poll.clear_fds();
    int fds = (int)(stdout_fd >= 0) + (int)(stderr_fd >= 0);
    poll.set_limit(fds);
    if (fds > 0)
    {
        poll.set_read_fd(stdout_fd);
        poll.set_read_fd(stderr_fd);
    }
}

} // namespace

/**
 * Wrapper PIPE
 */

namespace
{

enum FileDescAction
{
    None,
    File,
    FileAppend,
    Close,
};

struct StdFdWrapper
{
        int fd_read = -1;
        int fd_write = -1;
        FileDescAction action = None;
        std::function<void(std::string_view)> fun;
        std::string path;

        // fd utilities
        void add_pipe();
        // process fds once child process executed
        bool process_read_pipe();

        // fd redirections setting
        void close();
        void redirect_to(std::function<void(std::string_view)> && fun);
        void redirect_to(std::string & output);
        void redirect_to(Process::FileDescType fd);
        bool redirect_to_file(std::string_view path, bool append, mode_t open_mode);
        void reset();
        void zero_fd_read();
};

void StdFdWrapper::zero_fd_read()
{
    this->fd_read = -1;
}

void StdFdWrapper::add_pipe()
{
    if (this->fd_write >= 0)
        return;
    auto [fd_read, fd_write] = make_pipe();
    this->fd_read = fd_read;
    this->fd_write = fd_write;
}

bool StdFdWrapper::process_read_pipe()
{
    if (this->fd_read < 0)
        return true;
    if (this->action == File || this->action == FileAppend)
        return read_pipe_into_file(this->fd_read, this->path, this->action == FileAppend);
    else if (this->fun)
        return read_pipe_into_callback(this->fd_read, this->fun);
    return true;
}

void StdFdWrapper::reset()
{
    this->action = None;
    this->fun = nullptr;
    this->path.clear();
}

void StdFdWrapper::close()
{
    this->action = Close;
    safe_close(this->fd_read);
    safe_close(this->fd_write);
    this->fun = nullptr;
}

void StdFdWrapper::redirect_to(std::function<void(std::string_view)> && fun)
{
    this->add_pipe();
    this->fun = std::move(fun);
}

void StdFdWrapper::redirect_to(std::string & output)
{
    this->add_pipe();
    this->fun = [&output](std::string_view buffer) {
        output.append(buffer);
    };
}

void StdFdWrapper::redirect_to(Process::FileDescType fd)
{
    this->fun = nullptr;
    this->fd_write = fd;
}

bool StdFdWrapper::redirect_to_file(std::string_view path, bool append, mode_t open_mode)
{
    this->fun = nullptr;
    bool success = false;
    this->fd_write = open(path.data(), O_WRONLY | O_CREAT | (append ? O_APPEND : 0), open_mode);
    success = this->fd_write >= 0;
    if (success)
        this->action = append ? FileAppend : File;
    else
        SIHD_LOG(error, "Process: could not open output file: {}", path);
    return success;
}

struct PipeWrapper
{
        StdFdWrapper std_in;
        StdFdWrapper std_out;
        StdFdWrapper std_err;

        void reset();
};

void PipeWrapper::reset()
{
    std_in.reset();
    std_out.reset();
    std_err.reset();
}

struct ProcessWatcher
{
        std::mutex mutex;
        pid_t pid;
        int status;
        int code;

        void reset();
        bool has_terminated();
        void check_status(int options);
        Process::ReturnCodeType return_code();
};

void ProcessWatcher::check_status(int options)
{
    std::lock_guard l(this->mutex);
    siginfo_t info;
    int ret = waitid(P_PID, this->pid, &info, options);
    this->code = info.si_code;
    this->status = info.si_status;
    if (ret >= 0)
    {
        if (this->code == CLD_EXITED || this->code == CLD_KILLED)
            this->pid = -1;
    }
    else if (errno == ECHILD)
    {
        // child has exited
        this->pid = -1;
    }
    else if (errno == EINVAL)
    {
        SIHD_LOG_ERROR("Process: wait error: {}", os::last_error_str());
    }
}

bool ProcessWatcher::has_terminated()
{
    std::lock_guard l(this->mutex);
    return this->code == CLD_EXITED || this->code == CLD_KILLED;
}

void ProcessWatcher::reset()
{
    this->pid = -1;
    this->status = -1;
    this->code = -1;
}

Process::ReturnCodeType ProcessWatcher::return_code()
{
    std::lock_guard l(mutex);
    const bool has_exited = this->code == CLD_EXITED;
    return has_exited ? this->status : -1;
}

} // namespace

struct Process::Impl
{
        PipeWrapper pipe;
        ProcessWatcher process_watcher;
};

Process::Process()
{
    _impl = std::make_unique<Impl>();
    _impl->pipe.reset();
    _impl->process_watcher.reset();

    _started.store(false);
    _executing = false;
    _force_fork = false;
    _close_stdin_after_exec = false;
    _poll.set_timeout(5);
    _poll.add_observer(this);
    _poll.set_service_wait_stop(true);

    this->open_mode = SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE;

    this->clear();
}

Process::Process(std::function<int()> fun): Process()
{
    _fun_to_execute = fun;
}

Process::Process(std::span<const std::string> args): Process()
{
    _argv = std::vector<std::string>(args.begin(), args.end());
}

Process::Process(std::span<std::string_view> args): Process()
{
    _argv = std::vector<std::string>(args.begin(), args.end());
}

Process::Process(std::initializer_list<std::string_view> args): Process()
{
    _argv = std::vector<std::string>(args.begin(), args.end());
}

Process::Process(std::span<const char *> args): Process()
{
    _argv.reserve(args.size() + 1);
    for (const char *arg : args)
        _argv.emplace_back(arg);
}

Process::~Process()
{
    this->terminate();
    if (this->is_running())
    {
        this->stop();
        this->service_wait_stop();
    }
}

void Process::reset_proc()
{
    this->terminate();

    _impl->process_watcher.reset();
    _started.store(false);
}

void Process::clear()
{
    this->env_clear();
    this->env_load(str::table_span(environ));
    this->reset_proc();
    _impl->pipe.reset();
}

// Pipe stdin

Process & Process::stdin_close()
{
    _impl->pipe.std_in.close();
    return *this;
}

Process & Process::stdin_from(const std::string & input)
{
    _impl->pipe.std_in.add_pipe();
    write_into_pipe(_impl->pipe.std_in.fd_write, input);
    return *this;
}

Process & Process::stdin_from(FileDescType fd)
{
    _impl->pipe.std_in.fd_read = fd;
    return *this;
}

bool Process::stdin_from_file(std::string_view path)
{
    _impl->pipe.std_in.fd_read = open(path.data(), O_RDONLY);
    const bool success = _impl->pipe.std_in.fd_read >= 0;
    if (!success)
        SIHD_LOG(error, "Process: could not open file input: {}", path);
    return success;
}

// Pipe stdout

Process & Process::stdout_close()
{
    _impl->pipe.std_out.close();
    return *this;
}

Process & Process::stdout_to(std::function<void(std::string_view)> fun)
{
    _impl->pipe.std_out.redirect_to(std::move(fun));
    return *this;
}

Process & Process::stdout_to(std::string & output)
{
    _impl->pipe.std_out.redirect_to(output);
    return *this;
}

Process & Process::stdout_to(FileDescType fd)
{
    _impl->pipe.std_out.redirect_to(fd);
    return *this;
}

Process & Process::stdout_to(Process & proc)
{
    _impl->pipe.std_out.add_pipe();
    proc.stdin_from(_impl->pipe.std_out.fd_read);
    _impl->pipe.std_out.zero_fd_read();
    return *this;
}

bool Process::stdout_to_file(std::string_view path, bool append)
{
    return _impl->pipe.std_out.redirect_to_file(path, append, this->open_mode);
}

// Pipe stderr

Process & Process::stderr_close()
{
    _impl->pipe.std_err.close();
    return *this;
}

Process & Process::stderr_to(std::function<void(std::string_view)> fun)
{
    _impl->pipe.std_err.redirect_to(std::move(fun));
    return *this;
}

Process & Process::stderr_to(std::string & output)
{
    _impl->pipe.std_err.redirect_to(output);
    return *this;
}

Process & Process::stderr_to(FileDescType fd)
{
    _impl->pipe.std_err.redirect_to(fd);
    return *this;
}

Process & Process::stderr_to(Process & proc)
{
    _impl->pipe.std_err.add_pipe();
    proc.stdin_from(_impl->pipe.std_err.fd_read);
    _impl->pipe.std_err.zero_fd_read();
    return *this;
}

bool Process::stderr_to_file(std::string_view path, bool append)
{
    return _impl->pipe.std_err.redirect_to_file(path, append, this->open_mode);
}

// Execution

#if defined(ENABLE_FORK)

bool Process::_do_fork(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
    pid_t pid;
    if ((pid = fork()) < 0)
    {
        SIHD_LOG(error, "Process: fork failed: {}", os::last_error_str());
        return false;
    }
    if (pid == 0)
    {
        if (!_chroot.empty())
        {
            if (chroot(_chroot.c_str()) != 0)
            {
                SIHD_LOG(error, "Process: chroot failed: {}", os::last_error_str());
                _exit(2);
            }
        }

        if (!_chdir.empty())
        {
            if (!fs::chdir(_chdir.c_str()))
            {
                SIHD_LOG(error, "Process: chdir failed: {}", os::last_error_str());
                _exit(3);
            }
        }

        if (_impl->pipe.std_in.action == Close)
            close(STDIN_FILENO);
        else
        {
            dup_close(_impl->pipe.std_in.fd_read, STDIN_FILENO);
            safe_close(_impl->pipe.std_in.fd_write);
        }
        if (_impl->pipe.std_out.action == Close)
            close(STDOUT_FILENO);
        else
        {
            dup_close(_impl->pipe.std_out.fd_write, STDOUT_FILENO);
            safe_close(_impl->pipe.std_out.fd_read);
        }
        if (_impl->pipe.std_err.action == Close)
            close(STDERR_FILENO);
        else
        {
            dup_close(_impl->pipe.std_err.fd_write, STDERR_FILENO);
            safe_close(_impl->pipe.std_err.fd_read);
        }
        int status = 0;
        if (_fun_to_execute)
        {
            setup_environ_in_child_process(env);
            status = _fun_to_execute();
        }
        else
            status = execvpe(argv[0],
                             const_cast<char *const *>(&(argv[0])),
                             const_cast<char *const *>(&(env[0])));
        _exit(status);
    }
    _impl->process_watcher.pid = pid;
    return true;
}

#else

bool Process::_do_fork(const std::vector<const char *> &, const std::vector<const char *> &)
{
    return false;
}

#endif

#if defined(ENABLE_SPAWN)

bool Process::_do_spawn(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
    pid_t pid;
    posix_spawn_file_actions_t actions;

    posix_spawn_file_actions_init(&actions);

    if (!_chdir.empty())
        posix_spawn_file_actions_addchdir_np(&actions, _chdir.c_str());

    if (_impl->pipe.std_in.action == Close)
        add_close_action(&actions, STDIN_FILENO);
    else
    {
        add_dup_action(&actions, _impl->pipe.std_in.fd_read, STDIN_FILENO);
        add_close_action(&actions, _impl->pipe.std_in.fd_write);
    }
    if (_impl->pipe.std_out.action == Close)
        add_close_action(&actions, STDOUT_FILENO);
    else
    {
        add_dup_action(&actions, _impl->pipe.std_out.fd_write, STDOUT_FILENO);
        add_close_action(&actions, _impl->pipe.std_out.fd_read);
    }
    if (_impl->pipe.std_err.action == Close)
        add_close_action(&actions, STDERR_FILENO);
    else
    {
        add_dup_action(&actions, _impl->pipe.std_err.fd_write, STDERR_FILENO);
        add_close_action(&actions, _impl->pipe.std_err.fd_read);
    }
    int err = posix_spawnp(&pid,
                           argv[0],
                           &actions,
                           nullptr,
                           const_cast<char *const *>(&(argv[0])),
                           const_cast<char *const *>(&(env[0])));
    posix_spawn_file_actions_destroy(&actions);
    if (err != 0)
    {
        SIHD_LOG(error, "Process: '{}': {}", argv[0], os::last_error_str());
        return false;
    }
    _impl->process_watcher.pid = pid;
    return true;
}

#else

bool Process::_do_spawn(const std::vector<const char *> &, const std::vector<const char *> &)
{
    return false;
}

#endif

bool Process::_do_child_process(const std::vector<const char *> &, const std::vector<const char *> &)
{
    return false;
}

bool Process::_do_execute(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
    init_poller(_poll, _impl->pipe.std_out.fd_read, _impl->pipe.std_err.fd_read);
#if defined(ENABLE_SPAWN)
    if (_fun_to_execute || _force_fork)
        return this->_do_fork(argv, env);
    return this->_do_spawn(argv, env);
#else
    return this->_do_fork(argv, env);
#endif
}

bool Process::execute()
{
    if (this->is_process_running() || _executing.exchange(true) == true)
        return false;

    Defer d([this] { _executing.store(false); });

    if (!_fun_to_execute && _argv.size() == 0)
    {
        SIHD_LOG(error, "Process: Could not run process with no arguments");
        return false;
    }

    std::vector<const char *> c_argv;
    c_argv.reserve(_argv.size() + 1);
    for (const std::string & arg : _argv)
    {
        c_argv.emplace_back(arg.c_str());
    }
    c_argv.emplace_back(nullptr);

    std::vector<const char *> c_environ;
    c_environ.reserve(_environment.size() + 1);
    for (const std::string & env : _environment)
    {
        c_environ.emplace_back(env.c_str());
    }
    c_environ.emplace_back(nullptr);

    const bool success = this->_do_execute(c_argv, c_environ);
    if (success)
    {
        safe_close(_impl->pipe.std_in.fd_read);
        safe_close(_impl->pipe.std_out.fd_write);
        safe_close(_impl->pipe.std_err.fd_write);
    }

    if (_close_stdin_after_exec)
    {
        safe_close(_impl->pipe.std_in.fd_write);
    }

    _started.store(success);
    return success;
}

// Linux only methods

void Process::set_chroot(std::string_view path)
{
    _chroot = path;
}

void Process::set_force_fork(bool active)
{
    _force_fork = active;
}

pid_t Process::pid() const
{
    return _impl->process_watcher.pid;
}

bool Process::wait_process_end(Duration nano_duration)
{
    return _waitable.wait_for(nano_duration, [this] { return this->is_process_running() == false; });
}

bool Process::can_read_pipes() const
{
    return _poll.is_running() == false && _poll.fds_size() > 0;
}

bool Process::read_pipes(int milliseconds_timeout)
{
    return this->can_read_pipes() && _poll.poll(milliseconds_timeout) > 0;
}

bool Process::terminate()
{
    this->read_pipes();

    safe_close(_impl->pipe.std_in.fd_write);
    safe_close(_impl->pipe.std_out.fd_read);
    safe_close(_impl->pipe.std_err.fd_read);

    _poll.stop();
    _poll.clear_fds();

    int tries = 3;
    while (this->is_process_running() && tries > 0)
    {
        this->wait_no_hang();
        if (this->is_process_running() == false)
            break;
        --tries;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (this->is_process_running())
    {
        this->kill(SIGKILL);
        this->wait_exit(WUNTRACED);
    }

    return this->is_process_running() == false;
}

bool Process::kill(int sig)
{
    if (sig < 0)
    {
        sig = SIGTERM;
    }
    bool ret = this->is_process_running();
    if (ret)
    {
        ret = signal::kill(this->pid(), sig);
        if (!ret)
            SIHD_LOG(error, "Process: could not kill: {}", os::last_error_str());
    }
    return ret;
}

// Run

void Process::handle(Poll *poll)
{
    auto events = poll->events();
    for (auto & event : events)
    {
        int fd = event.fd;
        if (fd == _impl->pipe.std_out.fd_read)
        {
            if (!event.readable || _impl->pipe.std_out.process_read_pipe() == false)
            {
                poll->clear_fd(fd);
                safe_close(_impl->pipe.std_out.fd_read);
            }
        }
        else if (fd == _impl->pipe.std_err.fd_read)
        {
            if (!event.readable || _impl->pipe.std_err.process_read_pipe() == false)
            {
                poll->clear_fd(fd);
                safe_close(_impl->pipe.std_err.fd_read);
            }
        }
    }
    if (poll->polling_timeout())
    {
        if (this->is_process_running())
            this->wait_any(WNOHANG);
        else
            poll->stop();
    }
}

bool Process::on_start()
{
    const bool ret = this->execute();

    if (ret)
    {
        this->service_set_ready();
        if (_poll.max_fds() > 0)
        {
            _poll.start();
        }
    }

    return ret;
}

bool Process::on_stop()
{
    const bool ret = _poll.stop();
    this->reset_proc();
    return ret;
}

// Check process

bool Process::is_process_running() const
{
    return _started.load();
}

bool Process::wait_no_hang()
{
    return this->wait(WEXITED | WNOHANG);
}

bool Process::wait(int options)
{
    if (this->is_process_running() == false)
        return false;
    _impl->process_watcher.check_status(options);
    const bool terminated = _impl->process_watcher.has_terminated();
    if (terminated)
    {
        auto l = _waitable.guard();
        _started.store(false);
        _waitable.notify_all();
    }
    return terminated;
}

Process::ReturnCodeType Process::return_code() const
{
    return _impl->process_watcher.return_code();
}

bool Process::has_terminated() const
{
    return _impl->process_watcher.has_terminated();
}

bool Process::wait_exit(int options)
{
    return this->wait(WEXITED | options);
}

bool Process::wait_stop(int options)
{
    return this->wait(WSTOPPED | options);
}

bool Process::wait_continue(int options)
{
    return this->wait(WCONTINUED | options);
}

bool Process::wait_any(int options)
{
    return this->wait(WEXITED | WSTOPPED | WCONTINUED | options);
}

bool Process::has_exited() const
{
    std::lock_guard l(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_EXITED;
}

bool Process::has_core_dumped() const
{
    std::lock_guard l(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_DUMPED;
}

bool Process::has_stopped_by_signal() const
{
    std::lock_guard l(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_STOPPED;
}

bool Process::has_exited_by_signal() const
{
    std::lock_guard l(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_KILLED;
}

bool Process::has_continued() const
{
    std::lock_guard l(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_CONTINUED;
}

uint8_t Process::signal_exit_number() const
{
    const bool cond = this->has_exited_by_signal();
    std::lock_guard l(_impl->process_watcher.mutex);
    return cond ? _impl->process_watcher.status : -1;
}

uint8_t Process::signal_stop_number() const
{
    const bool cond = this->has_stopped_by_signal();
    std::lock_guard l(_impl->process_watcher.mutex);
    return cond ? _impl->process_watcher.status : -1;
}

} // namespace sihd::sys
