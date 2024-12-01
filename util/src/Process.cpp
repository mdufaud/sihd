#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Process.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/str.hpp>

#if !defined(__SIHD_WINDOWS__)

# if !defined(__ANDROID__)
#  include <spawn.h>
# endif

# include <fcntl.h>    // open
# include <sys/stat.h> // open
# include <sys/types.h>
# include <sys/wait.h>

# include <unistd.h>

#endif

#include <cerrno>
#include <cstdio>
#include <cstring> // strerror
#include <fstream>
#include <stdexcept>

#ifndef SIHD_PROCESS_READ_BUFFER_SIZE
# define SIHD_PROCESS_READ_BUFFER_SIZE 2048
#endif

#ifndef SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE
# define SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE 0740
#endif

extern "C"
{
extern char **environ;
}

namespace sihd::util
{

SIHD_LOGGER;

/**
 * Functions helper
 */

namespace
{

auto get_in_env(const std::vector<std::string> & env, std::string_view key)
{
    return container::find_if(env, [&key](const std::string & env) { return str::starts_with(env, key, "="); });
}

#if !defined(__SIHD_WINDOWS__)

# if !defined(__SIHD_ANDROID__) // no spawn on android

void add_dup_action(posix_spawn_file_actions_t *actions, int dup_from, int dup_to)
{
    if (dup_from >= 0 && dup_to >= 0)
    {
        posix_spawn_file_actions_adddup2(actions, dup_from, dup_to);
        posix_spawn_file_actions_addclose(actions, dup_from);
    }
}

void add_close_action(posix_spawn_file_actions_t *actions, int fd)
{
    if (fd >= 0)
        posix_spawn_file_actions_addclose(actions, fd);
}

# endif // __SIHD_ANDROID__

void build_fork_environ(const std::vector<const char *> & env)
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

std::pair<int, int> make_pipe()
{
    int fd[2];

    if (pipe(fd) < 0)
        throw std::runtime_error("Cannot make pipe");
    // read - write;
    return std::make_pair(fd[0], fd[1]);
}

bool read_fd_callback(int fd, std::function<void(std::string_view)> fun)
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

bool write_into_fd(int fd, const std::string & str)
{
    ssize_t ret = write(fd, str.c_str(), str.size());
    return ret >= 0 && ret == (ssize_t)str.size();
}

bool write_into_file(int fd, std::string & path, bool append)
{
    std::ofstream file(path, append ? std::ostream::app : std::ostream::out);

    auto fun = [&file](std::string_view buffer) {
        file << buffer;
    };
    if (!file.is_open() || !file.good())
        return false;
    return read_fd_callback(fd, fun);
}

void safe_close(int & fd)
{
    if (fd == -1)
        return;
    if (close(fd) == -1)
    {
        SIHD_LOG(error, "Process: could not close fd: {}", strerror(errno));
    }
    else
    {
        fd = -1;
    }
}

void dup_close(int fd_from, int fd_to)
{
    if (fd_from < 0)
        return;
    if (dup2(fd_from, fd_to) == -1)
    {
        SIHD_LOG(error, "Process: could not duplicate fd: {}", strerror(errno));
    }
    safe_close(fd_from);
}

#endif // __SIHD_WINDOWS__

void init_poller(sihd::util::Poll & poll, int stdout_fd, int stderr_fd)
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
        bool process_fd_out();

        // fd redirections setting
        void close();
        void redirect_to(std::function<void(std::string_view)> && fun);
        void redirect_to(std::string & output);
        void redirect_to(int fd);
        bool redirect_to_file(std::string_view path, bool append, mode_t open_mode);
        void reset();
};

void StdFdWrapper::add_pipe()
{
    if (this->fd_write != -1)
        return;
    auto [fd_read, fd_write] = make_pipe();
    this->fd_read = fd_read;
    this->fd_write = fd_write;
}

bool StdFdWrapper::process_fd_out()
{
    if (this->fd_read < 0)
        return true;
    if (this->action == File || this->action == FileAppend)
        return write_into_file(this->fd_read, this->path, this->action == FileAppend);
    else if (this->fun)
        return read_fd_callback(this->fd_read, this->fun);
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

void StdFdWrapper::redirect_to(int fd)
{
    this->fun = nullptr;
    this->fd_write = fd;
}

bool StdFdWrapper::redirect_to_file(std::string_view path, bool append, mode_t open_mode)
{
    this->fun = nullptr;
    this->fd_write = open(path.data(), O_WRONLY | O_CREAT | (append ? O_APPEND : 0), open_mode);
    if (this->fd_write >= 0)
        this->action = append ? FileAppend : File;
    else
        SIHD_LOG(error, "Process: could not open output file: {}", path);
    return this->fd_write >= 0;
}

} // namespace

struct Process::PipeWrapper
{
        StdFdWrapper std_in;
        StdFdWrapper std_out;
        StdFdWrapper std_err;

        void reset();
};

void Process::PipeWrapper::reset()
{
    std_in.reset();
    std_out.reset();
    std_err.reset();
}

Process::Process()
{
    _pipe_wrapper = std::make_unique<PipeWrapper>();

    _started.store(false);
    _executing = false;
    _force_fork = false;
    _close_stdin_after_exec = false;
    _pid = -1;
    _poll.set_timeout(5);
    _poll.add_observer(this);
    _poll.set_service_wait_stop(true);

    _code = 0;
    _status = 0;

    this->open_mode = SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE;

    this->clear();
}

Process::Process(std::function<int()> fun): Process()
{
    _fun_to_execute = fun;
}

Process::Process(const std::vector<std::string> & args): Process()
{
    _argv = args;
}

Process::Process(std::initializer_list<const char *> args): Process()
{
    _argv.reserve(args.size() + 1);
    std::copy(args.begin(), args.end(), std::back_inserter(_argv));
}

Process::Process(std::initializer_list<std::string> args): Process()
{
    _argv.reserve(args.size() + 1);
    std::copy(args.begin(), args.end(), std::back_inserter(_argv));
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

    _pid = -1;
    _started.store(false);

    {
        std::lock_guard l(_mutex_info);
        _code = 0;
        _status = 0;
    }
}

void Process::env_clear()
{
    _environment.clear();
}

void Process::env_load(const std::vector<std::string> & to_load_environ)
{
    for (const std::string & env : to_load_environ)
    {
        auto [key, value] = str::split_pair_view(env, "=");
        if (key.empty())
            continue;

        auto it = get_in_env(_environment, key);
        if (it != _environment.end())
            _environment.erase(it);
        _environment.emplace_back(env);
    }
}

void Process::env_load(const char **to_load_environ)
{
    if (to_load_environ == nullptr)
        return;
    int i = 0;
    while (to_load_environ[i] != nullptr)
    {
        auto [key, value] = str::split_pair_view(to_load_environ[i], "=");
        if (key.empty())
            continue;

        auto it = get_in_env(_environment, key);
        if (it != _environment.end())
            _environment.erase(it);
        _environment.emplace_back(to_load_environ[i]);
        ++i;
    }
}

void Process::env_set(std::string_view key, std::string_view value)
{
    std::string keyval = fmt::format("{}={}", key, value);
    for (std::string & env : _environment)
    {
        if (str::starts_with(env, key, "="))
        {
            env = std::move(keyval);
            return;
        }
    }
    _environment.emplace_back(std::move(keyval));
}

std::optional<std::string> Process::env_get(std::string_view key) const
{
    const auto it = get_in_env(_environment, key);
    if (it != _environment.end())
    {
        auto [_, value] = str::split_pair_view(*it, "=");
        return std::string(value);
    }
    return std::nullopt;
}

bool Process::env_rm(std::string_view key)
{
    auto it = get_in_env(_environment, key);
    if (it != _environment.end())
    {
        _environment.erase(it);
    }
    return it != _environment.end();
}

void Process::set_chroot(std::string_view path)
{
    _chroot = path;
}

void Process::set_chdir(std::string_view path)
{
    _chdir = path;
}

void Process::set_force_fork(bool active)
{
    _force_fork = active;
}

void Process::clear()
{
    this->env_clear();
    this->env_load(const_cast<const char **>(::environ));
    this->reset_proc();
    _pipe_wrapper->reset();
}

// Argv

Process & Process::set_function(std::nullptr_t)
{
    _fun_to_execute = nullptr;
    return *this;
}

Process & Process::set_function(std::function<int()> fun)
{
    _fun_to_execute = std::move(fun);
    return *this;
}

void Process::clear_argv()
{
    _argv.clear();
}

Process & Process::add_argv(const std::string & arg)
{
    _argv.push_back(arg);
    return *this;
}

Process & Process::add_argv(const std::vector<std::string> & args)
{
    _argv.insert(_argv.end(), args.begin(), args.end());
    return *this;
}

// Pipe stdin

Process & Process::stdin_close()
{
    _pipe_wrapper->std_in.close();
    return *this;
}

Process & Process::stdin_from(const std::string & input)
{
    _pipe_wrapper->std_in.add_pipe();
    write_into_fd(_pipe_wrapper->std_in.fd_write, input);
    return *this;
}

Process & Process::stdin_from(int fd)
{
    _pipe_wrapper->std_in.fd_read = fd;
    return *this;
}

bool Process::stdin_from_file(std::string_view path)
{
    _pipe_wrapper->std_in.fd_read = open(path.data(), O_RDONLY);
    if (_pipe_wrapper->std_in.fd_read < 0)
        SIHD_LOG(error, "Process: could not open file input: {}", path);
    return _pipe_wrapper->std_in.fd_read >= 0;
}

Process & Process::stdin_close_after_exec(bool activate)
{
    _close_stdin_after_exec = activate;
    return *this;
}

// Pipe stdout

Process & Process::stdout_close()
{
    _pipe_wrapper->std_out.close();
    return *this;
}

Process & Process::stdout_to(std::function<void(std::string_view)> fun)
{
    _pipe_wrapper->std_out.redirect_to(std::move(fun));
    return *this;
}

Process & Process::stdout_to(std::string & output)
{
    _pipe_wrapper->std_out.redirect_to(output);
    return *this;
}

Process & Process::stdout_to(int fd)
{
    _pipe_wrapper->std_out.redirect_to(fd);
    return *this;
}

Process & Process::stdout_to(Process & proc)
{
    _pipe_wrapper->std_out.add_pipe();
    proc.stdin_from(_pipe_wrapper->std_out.fd_read);
    _pipe_wrapper->std_out.fd_read = -1;
    return *this;
}

bool Process::stdout_to_file(std::string_view path, bool append)
{
    return _pipe_wrapper->std_out.redirect_to_file(path, append, this->open_mode);
}

// Pipe stderr

Process & Process::stderr_close()
{
    _pipe_wrapper->std_err.close();
    return *this;
}

Process & Process::stderr_to(std::function<void(std::string_view)> fun)
{
    _pipe_wrapper->std_err.redirect_to(std::move(fun));
    return *this;
}

Process & Process::stderr_to(std::string & output)
{
    _pipe_wrapper->std_err.redirect_to(output);
    return *this;
}

Process & Process::stderr_to(int fd)
{
    _pipe_wrapper->std_err.redirect_to(fd);
    return *this;
}

Process & Process::stderr_to(Process & proc)
{
    _pipe_wrapper->std_err.add_pipe();
    proc.stdin_from(_pipe_wrapper->std_err.fd_read);
    _pipe_wrapper->std_err.fd_read = -1;
    return *this;
}

bool Process::stderr_to_file(std::string_view path, bool append)
{
    return _pipe_wrapper->std_err.redirect_to_file(path, append, this->open_mode);
}

// Execution

bool Process::_do_fork(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
    pid_t pid;
    if ((pid = fork()) < 0)
    {
        SIHD_LOG(error, "Process: fork failed: {}", strerror(errno));
        return false;
    }
    if (pid == 0)
    {
        if (!_chroot.empty())
        {
            if (chroot(_chroot.c_str()) != 0)
            {
                SIHD_LOG(error, "Process: chroot failed: {}", strerror(errno));
                _exit(2);
            }
        }

        if (!_chdir.empty())
        {
            if (!fs::chdir(_chdir.c_str()))
            {
                SIHD_LOG(error, "Process: chdir failed: {}", strerror(errno));
                _exit(3);
            }
        }

        if (_pipe_wrapper->std_in.action == Close)
            close(STDIN_FILENO);
        else
        {
            dup_close(_pipe_wrapper->std_in.fd_read, STDIN_FILENO);
            safe_close(_pipe_wrapper->std_in.fd_write);
        }
        if (_pipe_wrapper->std_out.action == Close)
            close(STDOUT_FILENO);
        else
        {
            dup_close(_pipe_wrapper->std_out.fd_write, STDOUT_FILENO);
            safe_close(_pipe_wrapper->std_out.fd_read);
        }
        if (_pipe_wrapper->std_err.action == Close)
            close(STDERR_FILENO);
        else
        {
            dup_close(_pipe_wrapper->std_err.fd_write, STDERR_FILENO);
            safe_close(_pipe_wrapper->std_err.fd_read);
        }
        int status = 0;
        if (_fun_to_execute)
        {
            build_fork_environ(env);
            status = _fun_to_execute();
        }
        else
            status = execvpe(argv[0], const_cast<char *const *>(&(argv[0])), const_cast<char *const *>(&(env[0])));
        _exit(status);
    }
    _pid = pid;
    return true;
}

#if !defined(__SIHD_ANDROID__)
bool Process::_do_spawn(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
    pid_t pid;
    posix_spawn_file_actions_t actions;

    posix_spawn_file_actions_init(&actions);

    if (!_chdir.empty())
        posix_spawn_file_actions_addchdir_np(&actions, _chdir.c_str());

    if (_pipe_wrapper->std_in.action == Close)
        add_close_action(&actions, STDIN_FILENO);
    else
    {
        add_dup_action(&actions, _pipe_wrapper->std_in.fd_read, STDIN_FILENO);
        add_close_action(&actions, _pipe_wrapper->std_in.fd_write);
    }
    if (_pipe_wrapper->std_out.action == Close)
        add_close_action(&actions, STDOUT_FILENO);
    else
    {
        add_dup_action(&actions, _pipe_wrapper->std_out.fd_write, STDOUT_FILENO);
        add_close_action(&actions, _pipe_wrapper->std_out.fd_read);
    }
    if (_pipe_wrapper->std_err.action == Close)
        add_close_action(&actions, STDERR_FILENO);
    else
    {
        add_dup_action(&actions, _pipe_wrapper->std_err.fd_write, STDERR_FILENO);
        add_close_action(&actions, _pipe_wrapper->std_err.fd_read);
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
        SIHD_LOG(error, "Process: {}", strerror(errno));
        return false;
    }
    _pid = pid;
    return true;
}
#endif

bool Process::execute()
{
    if (this->is_process_running() || _executing.exchange(true) == true)
        return false;

    Defer d([this] { _executing.store(false); });

    if (!_fun_to_execute && _argv.size() == 0)
    {
        SIHD_LOG(error, "Process: Could not run process, no argv");
        return false;
    }

    init_poller(_poll, _pipe_wrapper->std_out.fd_read, _pipe_wrapper->std_err.fd_read);

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

    bool success = false;
    if (_fun_to_execute)
        success = this->_do_fork(c_argv, c_environ);
    else
    {
#if !defined(__SIHD_ANDROID__)
        if (_force_fork)
            success = this->_do_fork(c_argv, c_environ);
        else
            success = this->_do_spawn(c_argv, c_environ);
#else
        success = this->_do_fork(c_argv, c_environ);
#endif
    }

    if (success)
    {
        safe_close(_pipe_wrapper->std_in.fd_read);
        safe_close(_pipe_wrapper->std_out.fd_write);
        safe_close(_pipe_wrapper->std_err.fd_write);
    }

    if (_close_stdin_after_exec)
    {
        safe_close(_pipe_wrapper->std_in.fd_write);
    }

    _started.store(success);
    return success;
}

bool Process::wait_process_end(Timestamp nano_duration)
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

    safe_close(_pipe_wrapper->std_in.fd_write);
    safe_close(_pipe_wrapper->std_out.fd_read);
    safe_close(_pipe_wrapper->std_err.fd_read);

    _poll.stop();
    _poll.clear_fds();

    int tries = 3;
    while (this->is_process_running() && tries > 0)
    {
        this->wait_any(WNOHANG | WUNTRACED);
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
        sig = SIGTERM;
    bool ret = this->is_process_running();
    if (ret)
    {
        ret = ::kill(this->pid(), sig) >= 0;
        if (!ret)
            SIHD_LOG(error, "Process: could not kill: {}", strerror(errno));
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
        if (fd == _pipe_wrapper->std_out.fd_read)
        {
            if (!event.readable || _pipe_wrapper->std_out.process_fd_out() == false)
            {
                poll->clear_fd(fd);
                safe_close(_pipe_wrapper->std_out.fd_read);
            }
        }
        else if (fd == _pipe_wrapper->std_err.fd_read)
        {
            if (!event.readable || _pipe_wrapper->std_err.process_fd_out() == false)
            {
                poll->clear_fd(fd);
                safe_close(_pipe_wrapper->std_err.fd_read);
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
    bool ret = this->execute();

    if (ret && _poll.max_fds() > 0)
        _poll.start();

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

bool Process::wait(int options)
{
    if (this->is_process_running() == false)
        return false;
    int ret;
    {
        std::lock_guard l(_mutex_info);
        siginfo_t info;
        ret = waitid(P_PID, _pid, &info, options);
        _code = info.si_code;
        _status = info.si_status;
    }
    bool has_exited = false;
    if (ret >= 0)
    {
        if (this->has_exited() || this->has_exited_by_signal())
        {
            has_exited = true;
        }
    }
    else if (errno == ECHILD)
    {
        has_exited = true;
    }
    else if (errno == EINVAL)
    {
        SIHD_LOG_ERROR("Process: wait error: {}", strerror(errno));
    }
    if (has_exited)
    {
        auto l = _waitable.guard();
        _started.store(false);
        _pid = -1;
        _waitable.notify_all();
    }
    return has_exited;
}

bool Process::has_exited() const
{
    std::lock_guard l(_mutex_info);
    return _code == CLD_EXITED;
}

bool Process::has_core_dumped() const
{
    std::lock_guard l(_mutex_info);
    return _code == CLD_DUMPED;
}

bool Process::has_stopped_by_signal() const
{
    std::lock_guard l(_mutex_info);
    return _code == CLD_STOPPED;
}

bool Process::has_exited_by_signal() const
{
    std::lock_guard l(_mutex_info);
    return _code == CLD_KILLED;
}

bool Process::has_continued() const
{
    std::lock_guard l(_mutex_info);
    return _code == CLD_CONTINUED;
}

uint8_t Process::signal_exit_number() const
{
    const bool cond = this->has_exited_by_signal();
    std::lock_guard l(_mutex_info);
    return cond ? _status : -1;
}

uint8_t Process::signal_stop_number() const
{
    const bool cond = this->has_stopped_by_signal();
    std::lock_guard l(_mutex_info);
    return cond ? _status : -1;
}

uint8_t Process::return_code() const
{
    const bool cond = this->has_exited();
    std::lock_guard l(_mutex_info);
    return cond ? _status : -1;
}

} // namespace sihd::util
