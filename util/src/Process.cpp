#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Process.hpp>
#include <sihd/util/Timestamp.hpp>
#include <sihd/util/os.hpp>

#if !defined(__SIHD_WINDOWS__)

# if !defined(__ANDROID__)
#  include <spawn.h>
# endif

# include <fcntl.h>    // open
# include <sys/stat.h> // open
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>

# include <cerrno>
# include <cstdio>
# include <cstring> // strerror
# include <fstream>
# include <stdexcept>

# ifndef SIHD_PROCESS_READ_BUFFER_SIZE
#  define SIHD_PROCESS_READ_BUFFER_SIZE 2048
# endif

# ifndef SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE
#  define SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE 0740
# endif

namespace sihd::util
{

SIHD_LOGGER;

namespace
{
# if !defined(__SIHD_ANDROID__)

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

std::pair<int, int> make_pipe()
{
    int fd[2];

    if (pipe(fd) < 0)
        throw std::runtime_error("Cannot make pipe");
    // read - write;
    return std::make_pair(fd[0], fd[1]);
}

# endif

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

Process::Process()
{
    _started = false;
    _executing = false;
    _pid = -1;
    _poll.set_timeout(1);
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
    _started = false;

    {
        std::lock_guard l(_mutex_info);
        _code = 0;
        _status = 0;
    }
}

void Process::clear()
{
    constexpr auto reset_fwd = [](FileDescWrapper & fdw) {
        fdw.action = None;
        fdw.fun = nullptr;
        fdw.path.clear();
    };

    this->reset_proc();

    reset_fwd(_stdin);
    reset_fwd(_stdout);
    reset_fwd(_stderr);
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
    this->_fdw_close(_stdin);
    return *this;
}

Process & Process::stdin_from(const std::string & input)
{
    this->_add_pipe(_stdin);
    write_into_fd(_stdin.fd_write, input);
    return *this;
}

Process & Process::stdin_from(int fd)
{
    _stdin.fd_read = fd;
    return *this;
}

bool Process::stdin_from_file(std::string_view path)
{
    _stdin.fd_read = open(path.data(), O_RDONLY);
    if (_stdin.fd_read < 0)
        SIHD_LOG(error, "Process: could not open file input: {}", path);
    return _stdin.fd_read >= 0;
}

// Pipe stdout

Process & Process::stdout_close()
{
    this->_fdw_close(_stdout);
    return *this;
}

Process & Process::stdout_to(std::function<void(std::string_view)> fun)
{
    this->_fdw_to(_stdout, std::move(fun));
    return *this;
}

Process & Process::stdout_to(std::string & output)
{
    this->_fdw_to(_stdout, output);
    return *this;
}

Process & Process::stdout_to(int fd)
{
    this->_fdw_to(_stdout, fd);
    return *this;
}

Process & Process::stdout_to(Process & proc)
{
    this->_add_pipe(_stdout);
    proc.stdin_from(_stdout.fd_read);
    _stdout.fd_read = -1;
    return *this;
}

bool Process::stdout_to_file(std::string_view path, bool append)
{
    return this->_fdw_to_file(_stdout, path, append);
}

// Pipe stderr

Process & Process::stderr_close()
{
    this->_fdw_close(_stderr);
    return *this;
}

Process & Process::stderr_to(std::function<void(std::string_view)> fun)
{
    this->_fdw_to(_stderr, std::move(fun));
    return *this;
}

Process & Process::stderr_to(std::string & output)
{
    this->_fdw_to(_stderr, output);
    return *this;
}

Process & Process::stderr_to(int fd)
{
    this->_fdw_to(_stderr, fd);
    return *this;
}

Process & Process::stderr_to(Process & proc)
{
    this->_add_pipe(_stderr);
    proc.stdin_from(_stderr.fd_read);
    _stderr.fd_read = -1;
    return *this;
}

bool Process::stderr_to_file(std::string_view path, bool append)
{
    return this->_fdw_to_file(_stderr, path, append);
}

// Private fdw setters

void Process::_fdw_close(FileDescWrapper & fdw)
{
    fdw.action = Close;
    safe_close(fdw.fd_read);
    safe_close(fdw.fd_write);
    fdw.fun = nullptr;
}

void Process::_fdw_to(FileDescWrapper & fdw, std::function<void(std::string_view)> && fun)
{
    this->_add_pipe(fdw);
    fdw.fun = std::move(fun);
}

void Process::_fdw_to(FileDescWrapper & fdw, std::string & output)
{
    this->_add_pipe(fdw);
    fdw.fun = [&output](std::string_view buffer) {
        output.append(buffer);
    };
}

void Process::_fdw_to(FileDescWrapper & fdw, int fd)
{
    fdw.fun = nullptr;
    fdw.fd_write = fd;
}

bool Process::_fdw_to_file(FileDescWrapper & fdw, std::string_view path, bool append)
{
    fdw.fun = nullptr;
    fdw.fd_write = open(path.data(), O_WRONLY | O_CREAT | (append ? O_APPEND : 0), this->open_mode);
    if (fdw.fd_write >= 0)
        fdw.action = append ? FileAppend : File;
    else
        SIHD_LOG(error, "Process: could not open output file: {}", path);
    return fdw.fd_write >= 0;
}

// Private stuff

void Process::_add_pipe(FileDescWrapper & fdw)
{
    if (fdw.fd_write != -1)
        return;
    auto [fd_read, fd_write] = make_pipe();
    fdw.fd_read = fd_read;
    fdw.fd_write = fd_write;
}

// Execution

bool Process::_do_fork(const std::vector<const char *> & argv)
{
    pid_t pid;
    if ((pid = fork()) < 0)
    {
        SIHD_LOG(error, "Process: fork failed");
        return false;
    }
    if (pid == 0)
    {
        if (_stdin.action == Close)
            close(STDIN_FILENO);
        else
        {
            dup_close(_stdin.fd_read, STDIN_FILENO);
            safe_close(_stdin.fd_write);
        }
        if (_stdout.action == Close)
            close(STDOUT_FILENO);
        else
        {
            dup_close(_stdout.fd_write, STDOUT_FILENO);
            safe_close(_stdout.fd_read);
        }
        if (_stderr.action == Close)
            close(STDERR_FILENO);
        else
        {
            dup_close(_stderr.fd_write, STDERR_FILENO);
            safe_close(_stderr.fd_read);
        }
        int status = 0;
        if (_fun_to_execute)
            status = _fun_to_execute();
        else
            status = execvp(argv[0], const_cast<char *const *>(&(argv[0])));
        exit(status);
    }
    _pid = pid;
    return true;
}

# if !defined(__SIHD_ANDROID__)
bool Process::_do_spawn(const std::vector<const char *> & argv)
{
    pid_t pid;
    posix_spawn_file_actions_t actions;

    posix_spawn_file_actions_init(&actions);
    if (_stdin.action == Close)
        add_close_action(&actions, STDIN_FILENO);
    else
    {
        add_dup_action(&actions, _stdin.fd_read, STDIN_FILENO);
        add_close_action(&actions, _stdin.fd_write);
    }
    if (_stdout.action == Close)
        add_close_action(&actions, STDOUT_FILENO);
    else
    {
        add_dup_action(&actions, _stdout.fd_write, STDOUT_FILENO);
        add_close_action(&actions, _stdout.fd_read);
    }
    if (_stderr.action == Close)
        add_close_action(&actions, STDERR_FILENO);
    else
    {
        add_dup_action(&actions, _stderr.fd_write, STDERR_FILENO);
        add_close_action(&actions, _stderr.fd_read);
    }
    int err = posix_spawnp(&pid, argv[0], &actions, nullptr, const_cast<char *const *>(&(argv[0])), nullptr);
    posix_spawn_file_actions_destroy(&actions);
    if (err != 0)
    {
        SIHD_LOG(error, "Process: {}", strerror(errno));
        return false;
    }
    _pid = pid;
    return true;
}
# endif

bool Process::execute()
{
    if (this->is_process_running() || _executing.exchange(true) == true)
        return false;

    Defer d([this] { _executing = false; });

    if (!_fun_to_execute && _argv.size() == 0)
    {
        SIHD_LOG(error, "Process: Could not run process, no argv");
        return false;
    }

    init_poller(_poll, _stdout.fd_read, _stderr.fd_read);

    std::vector<const char *> c_argv;
    c_argv.reserve(_argv.size() + 1);
    for (const std::string & arg : _argv)
    {
        c_argv.emplace_back(arg.c_str());
    }
    c_argv.emplace_back(nullptr);

    bool success = false;
    if (_fun_to_execute)
        success = this->_do_fork(c_argv);
    else
    {
# if !defined(__SIHD_ANDROID__)
        success = this->_do_spawn(c_argv);
# else
        success = this->_do_fork(c_argv);
# endif
    }

    if (success)
    {
        safe_close(_stdin.fd_read);
        safe_close(_stdout.fd_write);
        safe_close(_stderr.fd_write);
    }

    _started = success;
    return success;
}

bool Process::wait_process_end(Timestamp nano_duration)
{
    return _waitable.wait_for(nano_duration, [this] { return this->is_process_running() == false; });
}

bool Process::_process_fd_out(FileDescWrapper & fdw)
{
    if (fdw.fd_read < 0)
        return true;
    if (fdw.action == File || fdw.action == FileAppend)
        return write_into_file(fdw.fd_read, fdw.path, fdw.action == FileAppend);
    else if (fdw.fun)
        return read_fd_callback(fdw.fd_read, fdw.fun);
    return true;
}

bool Process::read_pipes(int milliseconds_timeout)
{
    return _poll.is_running() == false && _poll.fds_size() > 0 && _poll.poll(milliseconds_timeout) > 0;
}

bool Process::terminate()
{
    this->read_pipes();

    safe_close(_stdin.fd_write);
    safe_close(_stdout.fd_read);
    safe_close(_stderr.fd_read);

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
        if (fd == _stdout.fd_read)
        {
            if (!event.readable || this->_process_fd_out(_stdout) == false)
            {
                poll->clear_fd(fd);
                safe_close(_stdout.fd_read);
            }
        }
        else if (fd == _stderr.fd_read)
        {
            if (!event.readable || this->_process_fd_out(_stderr) == false)
            {
                poll->clear_fd(fd);
                safe_close(_stderr.fd_read);
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

    if (_poll.max_fds() > 0)
        return ret && _poll.start();

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

int Process::signal_exit_number() const
{
    const bool cond = this->has_exited_by_signal();
    std::lock_guard l(_mutex_info);
    return cond ? _status : -1;
}

int Process::signal_stop_number() const
{
    const bool cond = this->has_stopped_by_signal();
    std::lock_guard l(_mutex_info);
    return cond ? _status : -1;
}

int Process::return_code() const
{
    const bool cond = this->has_exited();
    std::lock_guard l(_mutex_info);
    return cond ? _status : -1;
}

} // namespace sihd::util

#else

# pragma message("TODO")

#endif
