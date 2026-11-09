#include <sihd/sys/File.hpp>

#if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
# define ENABLE_SPAWN
# include <spawn.h>
#endif

#if !defined(__SIHD_EMSCRIPTEN__)
# define ENABLE_FORK
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <stdexcept>

#ifndef SIHD_PROCESS_READ_BUFFER_SIZE
# define SIHD_PROCESS_READ_BUFFER_SIZE 2048
#endif

#ifndef SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE
# define SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE 0740
#endif

#include <sihd/sys/Process.hpp>
#include <sihd/sys/SigThreadBlocker.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/str.hpp>

#include "internal/environment.hpp"

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
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

#if defined(ENABLE_SPAWN)

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

#endif // ENABLE_SPAWN

#if defined(ENABLE_FORK)

void dup_close(int fd_from, int fd_to)
{
    if (fd_from < 0)
        return;
    // dup2 onto itself keeps close-on-exec, which would close the std fd at exec
    if (fd_from == fd_to)
    {
        fcntl(fd_to, F_SETFD, 0);
        return;
    }
    if (dup2(fd_from, fd_to) == -1)
    {
        SIHD_LOG(error, "Process: could not duplicate fd: {}", os::last_error_str());
    }
    safe_close(fd_from);
}

#endif // ENABLE_FORK

std::pair<int, int> make_pipe()
{
    std::array<int, 2> fd;

    // other children must not inherit a pipe end: dup2 onto a std fd clears the flag
    if (pipe2(fd.data(), O_CLOEXEC) < 0)
        throw std::runtime_error("Cannot make pipe");
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

bool read_pipe_into_file(int fd, const std::string & path, bool append)
{
    File file(path, append ? "a" : "w");

    if (!file.is_open())
        return false;

    return read_pipe_into_callback(fd, [&file](std::string_view buffer) {
        SIHD_UNEXPECTED_LOG(file.write(buffer.data(), buffer.size()));
    });
}

void init_poller(sihd::sys::Poll & poll, int stdin_fd, int stdout_fd, int stderr_fd)
{
    poll.stop();
    poll.clear_fds();
    // stdin is only polled while input is queued: a pipe with room is always writable
    poll.set_limit((int)(stdin_fd >= 0) + (int)(stdout_fd >= 0) + (int)(stderr_fd >= 0));
    int fds = (int)(stdout_fd >= 0) + (int)(stderr_fd >= 0);
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
        // stdin only: input the child has not read yet, the write end closes once it is written
        std::string pending;
        bool close_after_flush = false;

        void add_pipe();
        bool process_read_pipe();
        bool flush_pending();

        void close();
        void redirect_to(std::function<void(std::string_view)> && fun);
        void redirect_to(std::string & output);
        void redirect_to(Process::FileDescType fd);
        bool redirect_to_file(const std::string & path, bool append, mode_t open_mode);
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

// false when the reader is gone, the input is dropped
bool StdFdWrapper::flush_pending()
{
#if !defined(__SIHD_EMSCRIPTEN__)
    // EPIPE instead of a SIGPIPE killing the process
    SigThreadBlocker blocker {SIGPIPE};
#endif
    while (!this->pending.empty())
    {
        const ssize_t written = write(this->fd_write, this->pending.data(), this->pending.size());
        if (written > 0)
            this->pending.erase(0, static_cast<size_t>(written));
        else if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return true;
        else if (written >= 0 || errno != EINTR)
        {
            this->pending.clear();
            return false;
        }
    }
    return true;
}

void StdFdWrapper::reset()
{
    this->action = None;
    this->fun = nullptr;
    this->path.clear();
    this->pending.clear();
    this->close_after_flush = false;
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

bool StdFdWrapper::redirect_to_file(const std::string & path, bool append, mode_t open_mode)
{
    this->fun = nullptr;
    bool success = false;
    this->fd_write = open(path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | (append ? O_APPEND : O_TRUNC), open_mode);
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

void flush_stdin(StdFdWrapper & std_in, sihd::sys::Poll & poll)
{
    if (std_in.fd_write < 0)
        return;
    const bool reader_alive = std_in.flush_pending();
    if (reader_alive && !std_in.pending.empty())
    {
        poll.set_write_fd(std_in.fd_write);
        return;
    }
    poll.clear_fd(std_in.fd_write);
    if (!reader_alive || std_in.close_after_flush)
        safe_close(std_in.fd_write);
}

struct ProcessWatcher
{
        std::mutex mutex;
        pid_t pid;
        int status;
        int code;
        bool reaped_elsewhere;

        void reset();
        bool has_terminated();
        bool check_status(int options);
        Process::ReturnCodeType return_code();
};

bool ProcessWatcher::check_status(int options)
{
    std::lock_guard lock(this->mutex);
    // race: a concurrent waiter already reaped it, waitid(P_PID, -1) is EINVAL
    if (this->pid <= 0)
        return true;
    // zeroed: WNOHANG with a live child leaves si_pid untouched, which means "no state change", not an error
    siginfo_t info {};
    int ret = waitid(P_PID, this->pid, &info, options);
    if (ret >= 0)
    {
        if (info.si_pid == 0)
            return true;

        this->code = info.si_code;
        this->status = info.si_status;
        if (this->code == CLD_EXITED || this->code == CLD_KILLED || this->code == CLD_DUMPED)
            this->pid = -1;
        return true;
    }
    if (errno == ECHILD)
    {
        // SIGCHLD ignored or a foreign waitpid: the child is gone, its status is lost
        this->pid = -1;
        this->reaped_elsewhere = true;
        return true;
    }
    return false;
}

bool ProcessWatcher::has_terminated()
{
    std::lock_guard lock(this->mutex);
    return this->reaped_elsewhere || this->code == CLD_EXITED || this->code == CLD_KILLED || this->code == CLD_DUMPED;
}

void ProcessWatcher::reset()
{
    this->pid = -1;
    this->status = -1;
    this->code = -1;
    this->reaped_elsewhere = false;
}

Process::ReturnCodeType ProcessWatcher::return_code()
{
    std::lock_guard lock(mutex);
    const bool has_exited = this->code == CLD_EXITED;
    return has_exited ? this->status : -1;
}

} // namespace

struct Process::Impl
{
        PipeWrapper pipe;
        ProcessWatcher process_watcher;
        // stdin is fed from the caller's thread while the poll service flushes it
        std::mutex stdin_mutex;
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
    _env = Environment::from_current();
    this->reset_proc();
    _impl->pipe.reset();
}

Process & Process::stdin_close()
{
    std::lock_guard lock(_impl->stdin_mutex);
    StdFdWrapper & std_in = _impl->pipe.std_in;
    if (this->is_process_running() && !std_in.pending.empty())
        std_in.close_after_flush = true;
    else
        std_in.close();
    return *this;
}

Process & Process::stdin_from(const std::string & input)
{
    std::lock_guard lock(_impl->stdin_mutex);
    StdFdWrapper & std_in = _impl->pipe.std_in;
    // a running child whose stdin is closed never reads again
    if (std_in.fd_write < 0 && this->is_process_running())
        return *this;
    if (std_in.fd_write < 0)
    {
        std_in.add_pipe();
        // the parent end only, the child reads a blocking pipe
        fcntl(std_in.fd_write, F_SETFL, fcntl(std_in.fd_write, F_GETFL) | O_NONBLOCK);
    }
    std_in.pending += input;
    if (this->is_process_running())
        flush_stdin(std_in, _poll);
    return *this;
}

Process & Process::stdin_from(FileDescType fd)
{
    _impl->pipe.std_in.fd_read = fd;
    return *this;
}

bool Process::stdin_from_file(const std::string & path)
{
    _impl->pipe.std_in.fd_read = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    const bool success = _impl->pipe.std_in.fd_read >= 0;
    if (!success)
        SIHD_LOG(error, "Process: could not open file input: {}", path);
    return success;
}

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

bool Process::stdout_to_file(const std::string & path, bool append)
{
    return _impl->pipe.std_out.redirect_to_file(path, append, this->open_mode);
}

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

bool Process::stderr_to_file(const std::string & path, bool append)
{
    return _impl->pipe.std_err.redirect_to_file(path, append, this->open_mode);
}

#if defined(ENABLE_FORK)

Result<void> Process::_do_fork(const std::vector<const char *> & argv)
{
    // the exec array is owned here: nothing may allocate in the forked child
    internal::ExecEnviron exec_env = internal::to_exec_environ(_env);
    pid_t pid;
    if ((pid = fork()) < 0)
        return std::unexpected(Error::from_errno("fork failed"));
    if (pid == 0)
    {
        if (!_chroot.empty())
        {
            if (chroot(_chroot.c_str()) != 0)
            {
                SIHD_LOG(error, "Process: chroot failed: {}", os::last_error_str());
                _exit(2);
            }
            // a cwd kept outside the new root reaches the real filesystem through ..
            if (!fs::chdir("/"))
            {
                SIHD_LOG(error, "Process: chdir to the new root failed: {}", os::last_error_str());
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
            // never let an exception unwind here: the child would run the parent's code as a second copy
            try
            {
                SIHD_UNEXPECTED_LOG(internal::apply(_env));
                status = _fun_to_execute();
            }
            catch (...)
            {
                SIHD_LOG(error, "Process: child function threw an exception");
                status = failure_return_code;
            }
        }
        else
            status = execvpe(argv[0],
                             const_cast<char *const *>(&(argv[0])),
                             const_cast<char *const *>(&(exec_env.array[0])));
        _exit(status);
    }
    _impl->process_watcher.pid = pid;
    return {};
}

#else

Result<void> Process::_do_fork(const std::vector<const char *> &)
{
    return std::unexpected(Error(ErrorCode::not_supported, "no fork on this platform"));
}

#endif

#if defined(ENABLE_SPAWN)

Result<void> Process::_do_spawn(const std::vector<const char *> & argv)
{
    internal::ExecEnviron exec_env = internal::to_exec_environ(_env);
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
                           const_cast<char *const *>(&(exec_env.array[0])));
    posix_spawn_file_actions_destroy(&actions);
    // posix_spawnp returns the error itself: errno is unrelated
    if (err != 0)
        return std::unexpected(Error(error_errno(err), "could not spawn '{}'", argv[0]));
    _impl->process_watcher.pid = pid;
    return {};
}

#else

Result<void> Process::_do_spawn(const std::vector<const char *> &)
{
    return std::unexpected(Error(ErrorCode::not_supported, "no spawn on this platform"));
}

#endif

Result<void> Process::_do_child_process(const std::vector<const char *> &)
{
    return std::unexpected(Error(ErrorCode::not_supported, "no child process on this platform"));
}

Result<void> Process::_do_execute(const std::vector<const char *> & argv)
{
    init_poller(_poll, _impl->pipe.std_in.fd_write, _impl->pipe.std_out.fd_read, _impl->pipe.std_err.fd_read);
#if defined(ENABLE_SPAWN)
    // posix_spawn cannot chroot
    if (_fun_to_execute || _force_fork || !_chroot.empty())
        return this->_do_fork(argv);
    return this->_do_spawn(argv);
#else
    return this->_do_fork(argv);
#endif
}

Result<void> Process::execute()
{
    if (this->is_process_running() || _executing.exchange(true) == true)
    {
        _executing.store(false);
        return std::unexpected(Error(ErrorCode::not_initialized, "process already running or executing"));
    }

    Defer defer([this] { _executing.store(false); });

    if (!_fun_to_execute && _argv.size() == 0)
        return std::unexpected(Error(ErrorCode::invalid_argument, "could not run process with no arguments"));

    std::vector<const char *> c_argv;
    c_argv.reserve(_argv.size() + 1);
    for (const std::string & arg : _argv)
    {
        c_argv.emplace_back(arg.c_str());
    }
    c_argv.emplace_back(nullptr);

    // the child closes each source after its dup2: one fd shared by stdout and stderr needs a copy per stream
    StdFdWrapper & std_err = _impl->pipe.std_err;
    if (std_err.fd_write >= 0 && std_err.fd_write == _impl->pipe.std_out.fd_write)
    {
        std_err.fd_write = fcntl(std_err.fd_write, F_DUPFD_CLOEXEC, 0);
        if (std_err.fd_write < 0)
            return std::unexpected(Error::from_errno("could not duplicate the descriptor shared by stdout and stderr"));
    }

    auto executed = this->_do_execute(c_argv);
    // the child ends belong to the child, whether it started or not
    safe_close(_impl->pipe.std_in.fd_read);
    safe_close(_impl->pipe.std_out.fd_write);
    safe_close(_impl->pipe.std_err.fd_write);
    if (executed)
        this->_apply_child_policy();

    {
        std::lock_guard lock(_impl->stdin_mutex);
        StdFdWrapper & std_in = _impl->pipe.std_in;
        std_in.close_after_flush = _close_stdin_after_exec;
        if (executed)
            flush_stdin(std_in, _poll);
        else if (_close_stdin_after_exec)
            safe_close(std_in.fd_write);
    }

    _started.store(executed.has_value());
    if (!executed)
        return executed;
    return {};
}

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
    if (_poll.is_running())
        return false;
    // the poll keeps its slots: what counts is a captured pipe not at its end of file, or queued input
    if (_impl->pipe.std_out.fd_read >= 0 || _impl->pipe.std_err.fd_read >= 0)
        return true;
    std::lock_guard lock(_impl->stdin_mutex);
    return !_impl->pipe.std_in.pending.empty();
}

bool Process::read_pipes(int milliseconds_timeout)
{
    return this->can_read_pipes() && _poll.poll(milliseconds_timeout) > 0;
}

bool Process::terminate()
{
    // a read takes one chunk per pipe: drain the default 64 KiB pipe a finished child may have filled
    int polls = 65536 / SIHD_PROCESS_READ_BUFFER_SIZE;
    while (polls-- > 0 && this->read_pipes())
        ;

    {
        std::lock_guard lock(_impl->stdin_mutex);
        _impl->pipe.std_in.pending.clear();
        safe_close(_impl->pipe.std_in.fd_write);
    }
    safe_close(_impl->pipe.std_out.fd_read);
    safe_close(_impl->pipe.std_err.fd_read);

    _poll.stop();
    _poll.clear_fds();

    int tries = 3;
    while (this->is_process_running() && tries > 0)
    {
        (void)this->wait_no_hang();
        if (this->is_process_running() == false)
            break;
        --tries;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (this->is_process_running())
    {
        (void)this->kill(SIGKILL);
        (void)this->wait_exit(WUNTRACED);
    }

    return this->is_process_running() == false;
}

Result<void> Process::kill(int sig)
{
    // read once: a concurrent reap sets it to -1, and pid <= 0 signals a group or every user process
    const pid_t pid = this->pid();
    if (this->is_process_running() == false || pid <= 0)
        return std::unexpected(Error(ErrorCode::not_initialized, "process is not running"));
    if (sig < 0)
        sig = SIGTERM;
    return signal::kill(pid, sig);
}

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
        else
        {
            // writable, or the reader closed its end: both end in flush_stdin
            std::lock_guard lock(_impl->stdin_mutex);
            if (fd == _impl->pipe.std_in.fd_write)
                flush_stdin(_impl->pipe.std_in, *poll);
        }
    }
    if (poll->polling_timeout())
    {
        if (this->is_process_running())
            (void)this->wait_any(WNOHANG);
        else
            poll->stop();
    }
}

bool Process::on_start()
{
    auto executed = this->execute();
    if (SIHD_UNEXPECTED_LOG(executed))
        return false;

    this->service_set_ready();
    // without any captured pipe the poll times out, which reaps the child and ends the loop
    _poll.start();

    return true;
}

bool Process::on_stop()
{
    const bool ret = _poll.stop();
    this->reset_proc();
    return ret;
}

bool Process::is_process_running() const
{
    return _started.load();
}

Result<bool> Process::wait_no_hang()
{
    return this->wait(WEXITED | WNOHANG);
}

Result<bool> Process::wait(int options)
{
    if (this->is_process_running() == false)
        return false;
    if (_impl->process_watcher.check_status(options) == false)
        return std::unexpected(Error::from_errno("wait"));
    const bool terminated = _impl->process_watcher.has_terminated();
    if (terminated)
    {
        auto guard = _waitable.guard();
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

Result<bool> Process::wait_exit(int options)
{
    return this->wait(WEXITED | options);
}

Result<bool> Process::wait_stop(int options)
{
    return this->wait(WSTOPPED | options);
}

Result<bool> Process::wait_continue(int options)
{
    return this->wait(WCONTINUED | options);
}

Result<bool> Process::wait_any(int options)
{
    return this->wait(WEXITED | WSTOPPED | WCONTINUED | options);
}

bool Process::has_exited() const
{
    std::lock_guard lock(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_EXITED;
}

bool Process::has_core_dumped() const
{
    std::lock_guard lock(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_DUMPED;
}

bool Process::has_stopped_by_signal() const
{
    std::lock_guard lock(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_STOPPED;
}

bool Process::has_exited_by_signal() const
{
    std::lock_guard lock(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_KILLED;
}

bool Process::has_continued() const
{
    std::lock_guard lock(_impl->process_watcher.mutex);
    return _impl->process_watcher.code == CLD_CONTINUED;
}

uint8_t Process::signal_exit_number() const
{
    const bool cond = this->has_exited_by_signal();
    std::lock_guard lock(_impl->process_watcher.mutex);
    return cond ? _impl->process_watcher.status : -1;
}

uint8_t Process::signal_stop_number() const
{
    const bool cond = this->has_stopped_by_signal();
    std::lock_guard lock(_impl->process_watcher.mutex);
    return cond ? _impl->process_watcher.status : -1;
}

} // namespace sihd::sys
