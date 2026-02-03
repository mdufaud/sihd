#include <sihd/util/platform.hpp>

#if !defined(__SIHD_WINDOWS__)

# if !defined(__SIHD_ANDROID__) && !defined(__SIHD_EMSCRIPTEN__)
#  include <spawn.h>
#  define ENABLE_SPAWN
# endif

# if !defined(__SIHD_EMSCRIPTEN__)
#  define ENABLE_FORK
# endif

# include <fcntl.h>    // open
# include <sys/stat.h> // open
# include <sys/types.h>
# include <sys/wait.h>

# include <unistd.h>

#endif

#include <cerrno>
#include <cstdio>
#include <stdexcept>

#ifndef SIHD_PROCESS_READ_BUFFER_SIZE
# define SIHD_PROCESS_READ_BUFFER_SIZE 2048
#endif

#ifndef SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE
# define SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE 0740
#endif

#include <sihd/util/Defer.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Process.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/container.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/signal.hpp>
#include <sihd/util/str.hpp>

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
    return container::find_if(env,
                              [&key](const std::string & env) { return str::starts_with(env, key, "="); });
}

#if !defined(__SIHD_WINDOWS__)

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

#else // __SIHD_WINDOWS__

std::pair<HANDLE, HANDLE> make_pipe()
{
    HANDLE rd;
    HANDLE rw;
    SECURITY_ATTRIBUTES saAttr;

    // Set the bInheritHandle flag so pipe handles are inherited.

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&rd, &rw, &saAttr, 0))
        throw std::runtime_error("Cannot make pipe: CreatePipe");

    if (!SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(rd);
        CloseHandle(rw);
        throw std::runtime_error("Cannot make pipe: SetHandleInformation");
    }

    // enables peeking pipe
    DWORD mode = PIPE_NOWAIT;
    if (!SetNamedPipeHandleState(rd, &mode, NULL, NULL))
    {
        CloseHandle(rd);
        CloseHandle(rw);
        throw std::runtime_error("Cannot make pipe: SetNamedPipeHandleState");
    }

    // read - write;
    return std::make_pair(rd, rw);
}

void safe_close(HANDLE & fd)
{
    if (fd == nullptr)
        return;
    if (!CloseHandle(fd))
    {
        SIHD_LOG(error, "Process: could not close fd: {}", os::last_error_str());
    }
    else
    {
        fd = nullptr;
    }
}

bool read_pipe_into_callback(HANDLE fd, std::function<void(std::string_view)> fun)
{
    DWORD total_bytes_avail;
    DWORD bytes_left;

    if (!PeekNamedPipe(fd, NULL, 0, NULL, &total_bytes_avail, &bytes_left))
        return false;
    if (total_bytes_avail <= 0)
        return false;

    char buffer[SIHD_PROCESS_READ_BUFFER_SIZE + 1];
    DWORD read;
    BOOL success = ReadFile(fd, &buffer, SIHD_PROCESS_READ_BUFFER_SIZE, &read, NULL);
    if (success)
    {
        buffer[read] = 0;
        fun(std::string_view {buffer, (size_t)read});
    }
    return success;
}

bool write_into_pipe(HANDLE fd, const std::string & str)
{
    DWORD written;
    BOOL success = WriteFile(fd, str.c_str(), str.size(), &written, NULL);
    return success && written == (ssize_t)str.size();
}

bool read_pipe_into_file(HANDLE fd, const std::string & path, bool append)
{
    File file(path, append ? "a" : "w");

    if (!file.is_open())
        return false;

    auto fun = [&file](std::string_view buffer) {
        file.write(buffer.data(), buffer.size());
    };
    return read_pipe_into_callback(fd, fun);
}

#endif // Difference LINUX / WINDOWS

template <typename T>
void env_load_impl(const T & to_load_environ, std::vector<std::string> & environment)
{
    for (const auto & env : to_load_environ)
    {
        auto [key, value] = str::split_pair_view(env, "=");
        if (key.empty())
            continue;

        auto it = get_in_env(environment, key);
        if (it != environment.end())
            environment.erase(it);
        environment.emplace_back(env);
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
#if !defined(__SIHD_WINDOWS__)
        int fd_read = -1;
        int fd_write = -1;
#else
        HANDLE fd_read = nullptr;
        HANDLE fd_write = nullptr;
#endif
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
#if !defined(__SIHD_WINDOWS__)
    this->fd_read = -1;
#else
    this->fd_read = nullptr;
#endif
}

void StdFdWrapper::add_pipe()
{
#if !defined(__SIHD_WINDOWS__)
    if (this->fd_write >= 0)
#else
    if (this->fd_write != nullptr)
#endif
        return;
    auto [fd_read, fd_write] = make_pipe();
    this->fd_read = fd_read;
    this->fd_write = fd_write;
}

bool StdFdWrapper::process_read_pipe()
{
#if !defined(__SIHD_WINDOWS__)
    if (this->fd_read < 0)
#else
    if (this->fd_read == nullptr)
#endif
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
#if !defined(__SIHD_WINDOWS__)
    this->fd_write = open(path.data(), O_WRONLY | O_CREAT | (append ? O_APPEND : 0), open_mode);
    success = this->fd_write >= 0;
#else
# pragma message("TODO CreateFile permissions in windows")
    (void)open_mode;
    this->fd_write = CreateFile(path.data(),
                                GENERIC_WRITE,
                                0,
                                NULL,
                                (append ? OPEN_ALWAYS : CREATE_ALWAYS),
                                FILE_ATTRIBUTE_READONLY,
                                NULL);
    success = this->fd_write != nullptr;
#endif
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
#if !defined(__SIHD_WINDOWS__)
        pid_t pid;
        int status;
        int code;
#else
        DWORD code;
        bool exited;
        PROCESS_INFORMATION procinfo;
#endif

        void reset();
        bool has_terminated();
        void check_status(int options);
        Process::ReturnCodeType return_code();
};

void ProcessWatcher::check_status(int options)
{
    std::lock_guard l(this->mutex);
#if !defined(__SIHD_WINDOWS__)
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
#else
    const DWORD timeout_ms = options == 0 ? INFINITE : options;
    const DWORD result = WaitForSingleObject(this->procinfo.hProcess, timeout_ms);
    if (result == WAIT_OBJECT_0)
    {
        // The child process was terminated
        if (GetExitCodeProcess(this->procinfo.hProcess, &this->code))
        {
            this->exited = true;

            if (this->procinfo.hProcess != nullptr)
                CloseHandle(this->procinfo.hProcess);
            this->procinfo.hProcess = nullptr;

            if (this->procinfo.hThread != nullptr)
                CloseHandle(this->procinfo.hThread);
            this->procinfo.hThread = nullptr;
        }
        else
        {
            SIHD_LOG_ERROR("Process: GetExitCodeProcess: {}", os::last_error_str());
        }
    }
    else if (result == WAIT_TIMEOUT)
    {
        // The child process is still running
        this->exited = false;
    }
    else
    {
        SIHD_LOG_ERROR("Process: WaitForSingleObject: {}", os::last_error_str());
    }
#endif
}

bool ProcessWatcher::has_terminated()
{
    std::lock_guard l(this->mutex);
#if !defined(__SIHD_WINDOWS__)
    return this->code == CLD_EXITED || this->code == CLD_KILLED;
#else
    return this->exited;
#endif
}

void ProcessWatcher::reset()
{
#if !defined(__SIHD_WINDOWS__)
    this->pid = -1;
    this->status = -1;
    this->code = -1;
#else
    this->code = -1;
    this->exited = false;
    if (this->procinfo.hProcess != nullptr)
        CloseHandle(this->procinfo.hProcess);
    if (this->procinfo.hThread != nullptr)
        CloseHandle(this->procinfo.hThread);
    ZeroMemory(&this->procinfo, sizeof(PROCESS_INFORMATION));
#endif
}

Process::ReturnCodeType ProcessWatcher::return_code()
{
#if !defined(__SIHD_WINDOWS__)
    std::lock_guard l(mutex);
    const bool has_exited = this->code == CLD_EXITED;
    return has_exited ? this->status : -1;
#else
    return this->exited ? this->code : -1;
#endif
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

void Process::env_clear()
{
    _environment.clear();
}

void Process::env_load(std::span<const std::string> to_load_environ)
{
    env_load_impl(to_load_environ, _environment);
}

void Process::env_load(std::span<const char *> to_load_environ)
{
    env_load_impl(to_load_environ, _environment);
}

void Process::env_load(std::span<std::string_view> to_load_environ)
{
    env_load_impl(to_load_environ, _environment);
}

void Process::env_load(std::initializer_list<std::string_view> to_load_environ)
{
    env_load_impl(to_load_environ, _environment);
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

void Process::set_chdir(std::string_view path)
{
    _chdir = path;
}

#if !defined(__SIHD_WINDOWS__)

void Process::set_chroot(std::string_view path)
{
    _chroot = path;
}

void Process::set_force_fork(bool active)
{
    _force_fork = active;
}

#endif

void Process::clear()
{
    this->env_clear();
    this->env_load(str::table_span(environ));
    this->reset_proc();
    _impl->pipe.reset();
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

Process & Process::add_argv(std::string_view arg)
{
    _argv.emplace_back(arg);
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
    bool success = false;
#if !defined(__SIHD_WINDOWS__)
    _impl->pipe.std_in.fd_read = open(path.data(), O_RDONLY);
    success = _impl->pipe.std_in.fd_read >= 0;
#else
    _impl->pipe.std_in.fd_read
        = CreateFile(path.data(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    success = _impl->pipe.std_in.fd_read != nullptr;
#endif
    if (!success)
        SIHD_LOG(error, "Process: could not open file input: {}", path);
    return success;
}

Process & Process::stdin_close_after_exec(bool activate)
{
    _close_stdin_after_exec = activate;
    return *this;
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

bool Process::_do_fork(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
#if defined(__SIHD_WINDOWS__) || !defined(ENABLE_FORK)
    (void)argv;
    (void)env;
    return false;
#else
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
#endif
}

bool Process::_do_spawn(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
#if !defined(ENABLE_SPAWN)
    (void)argv;
    (void)env;
    return false;
#else
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
#endif
}

bool Process::_do_child_process(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
#if !defined(__SIHD_WINDOWS__)
    (void)argv;
    (void)env;
    return false;
#else
    STARTUPINFO start_info;
    BOOL success = FALSE;

    // Set up members of the STARTUPINFO structure.
    ZeroMemory(&start_info, sizeof(STARTUPINFO));
    start_info.cb = sizeof(STARTUPINFO);

    if (_impl->pipe.std_in.fd_read != nullptr)
        start_info.hStdInput = _impl->pipe.std_in.fd_read;
    else if (_impl->pipe.std_in.action != Close)
        start_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    if (_impl->pipe.std_out.fd_write != nullptr)
        start_info.hStdOutput = _impl->pipe.std_out.fd_write;
    else if (_impl->pipe.std_out.action != Close)
        start_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    if (_impl->pipe.std_err.fd_write != nullptr)
        start_info.hStdError = _impl->pipe.std_err.fd_write;
    else if (_impl->pipe.std_err.action != Close)
        start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    start_info.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmd_line;
    for (const char *arg : argv)
    {
        if (arg == nullptr)
            break;
        cmd_line += arg;
        cmd_line += " ";
    }

    std::string env_str;
    for (const char *val : env)
    {
        if (val == nullptr)
            break;
        env_str += val;
        env_str += "\0";
    }
    env_str += "\0";

    // Create the child process.

    // const DWORD creation_flags = NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW;
    const DWORD creation_flags = 0;

    success = CreateProcess(NULL,
                            cmd_line.data(), // command line
                            NULL,            // process security attributes
                            NULL,            // primary thread security attributes
                            TRUE,            // handles are inherited
                            creation_flags,  // creation flags
                            env_str.empty() ? NULL : (LPVOID)env_str.data(), // environment
                            _chdir.empty() ? NULL : _chdir.data(),           // current directory
                            &start_info,                                     // STARTUPINFO pointer
                            &_impl->process_watcher.procinfo);               // receives PROCESS_INFORMATION

    // If an error occurs, exit the application.
    if (!success)
    {
        SIHD_LOG(error, "Process: {}", os::last_error_str());
        return false;
    }
    else
    {
        // Close handles to the child process and its primary thread.
        // Some applications might keep these handles to monitor the status
        // of the child process
    }
    return success;
#endif
}

bool Process::_do_execute(const std::vector<const char *> & argv, const std::vector<const char *> & env)
{
#if defined(__SIHD_WINDOWS__)
    return this->_do_child_process(argv, env);
#else
    init_poller(_poll, _impl->pipe.std_out.fd_read, _impl->pipe.std_err.fd_read);
# if defined(ENABLE_SPAWN)
    if (_fun_to_execute || _force_fork)
        return this->_do_fork(argv, env);
    return this->_do_spawn(argv, env);
# else
    return this->_do_fork(argv, env);
# endif
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

#if !defined(__SIHD_WINDOWS__)

pid_t Process::pid() const
{
    return _impl->process_watcher.pid;
}

#else

DWORD Process::pid() const
{
    return _impl->process_watcher.procinfo.dwProcessId;
}

HANDLE Process::process() const
{
    return _impl->process_watcher.procinfo.hProcess;
}
#endif

bool Process::wait_process_end(Timestamp nano_duration)
{
    return _waitable.wait_for(nano_duration, [this] { return this->is_process_running() == false; });
}

bool Process::can_read_pipes() const
{
#if !defined(__SIHD_WINDOWS__)
    return _poll.is_running() == false && _poll.fds_size() > 0;
#else
    return _impl->pipe.std_in.fd_read != nullptr || _impl->pipe.std_out.fd_read != nullptr
           || _impl->pipe.std_err.fd_read != nullptr;
#endif
}

bool Process::read_pipes(int milliseconds_timeout)
{
#if !defined(__SIHD_WINDOWS__)
    return this->can_read_pipes() && _poll.poll(milliseconds_timeout) > 0;
#else
    if (this->can_read_pipes() == false)
        return false;
    SteadyClock clock;
    const time_t begin = clock.now();
    const time_t timeout = time::milliseconds(milliseconds_timeout);
    bool timed_out = false;
    while (!timed_out)
    {
        _impl->pipe.std_out.process_read_pipe();
        _impl->pipe.std_err.process_read_pipe();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        timed_out = (clock.now() - begin) >= timeout;
    }
    return true;
#endif
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
#if !defined(__SIHD_WINDOWS__)
        this->kill(SIGKILL);
        this->wait_exit(WUNTRACED);
#else
        this->kill();
        this->wait();
#endif
    }

    return this->is_process_running() == false;
}

bool Process::kill(int sig)
{
    if (sig < 0)
    {
#if !defined(__SIHD_WINDOWS__)
        sig = SIGTERM;
#else
        sig = 15;
#endif
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
#if !defined(__SIHD_WINDOWS__)
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
#else
    (void)poll;
#endif
}

bool Process::on_start()
{
    const bool ret = this->execute();

    if (ret)
    {
        this->service_set_ready();
#if !defined(__SIHD_WINDOWS__)
        if (_poll.max_fds() > 0)
        {
            _poll.start();
        }
#else
        while (this->is_running())
        {
            constexpr DWORD timeout_ms = 50;
            this->read_pipes(timeout_ms);
        }
#endif
    }

    return ret;
}

bool Process::on_stop()
{
#if !defined(__SIHD_WINDOWS__)
    const bool ret = _poll.stop();
    this->reset_proc();
    return ret;
#else
    this->reset_proc();
    return true;
#endif
}

// Check process

bool Process::is_process_running() const
{
    return _started.load();
}

bool Process::wait_no_hang()
{
#if !defined(__SIHD_WINDOWS__)
    return this->wait(WEXITED | WNOHANG);
#else
    constexpr DWORD timeout_ms = 5;
    return this->wait(timeout_ms);
#endif
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

#if !defined(__SIHD_WINDOWS__)

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

#else

#endif

} // namespace sihd::util
