#include <sihd/util/Process.hpp>
#include <sihd/util/Logger.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <string.h> // strerror
# include <errno.h>
# include <unistd.h>
# include <sys/wait.h>
# include <sys/types.h>
# include <sys/stat.h> // open
# include <fcntl.h> // open

# include <stdexcept>
# include <fstream>

# ifndef SIHD_PROCESS_READ_BUFFER_SIZE
#  define SIHD_PROCESS_READ_BUFFER_SIZE 2048
# endif

# ifndef SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE
#  define SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE 0740
# endif

namespace sihd::util
{

LOGGER;

Process::Process()
{
    _pid = -1;
    this->open_mode = SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE;
    this->clear();
}

Process::Process(std::function<int()> fun)
{
    _pid = -1;
    this->open_mode = SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE;
    this->clear();
    _fun_to_execute = fun;
}

Process::Process(const std::vector<std::string> & args)
{
    _pid = -1;
    this->open_mode = SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE;
    this->clear();
    _argv.reserve(args.size() + 1);
    for (const std::string & arg: args)
        _argv.push_back(arg.c_str());
}

Process::Process(std::initializer_list<const char *> args)
{
    _pid = -1;
    this->open_mode = SIHD_PROCESS_OUTPUT_FILE_DEFAULT_MODE;
    this->clear();
    _argv.reserve(args.size() + 1);
    std::copy(args.begin(), args.end(), std::back_inserter(_argv));
}

Process::~Process()
{
    this->end();
    this->wait();
}

void    Process::clear()
{
    this->end();
    this->wait();
    _pid = -1;
    _status.reset();
    this->_clear(_stdin);
    this->_clear(_stdout);
    this->_clear(_stderr);
}

void    Process::_clear(FileDescWrapper & fdw)
{
    fdw.action = NONE;
    fdw.fun = nullptr;
    fdw.str_out.reset();
}

// Argv

Process &   Process::set_function(std::nullptr_t)
{
    _fun_to_execute = nullptr;
    return *this;
}

Process &   Process::set_function(std::function<int()> fun)
{
    _fun_to_execute = std::move(fun);
    return *this;
}

void    Process::clear_argv()
{
    _argv.clear();
}

Process &   Process::add_argv(const std::string & arg)
{
    if (_argv.size() > 0 && _argv.back() == NULL)
        _argv.pop_back();
    _argv.push_back(arg.c_str());
    return *this;
}

Process &   Process::add_argv(const std::vector<std::string> & args)
{
    if (_argv.size() > 0 && _argv.back() == NULL)
        _argv.pop_back();
    _argv.reserve(_argv.size() + args.size() + 1);
    for (const std::string & arg: args)
        _argv.push_back(arg.c_str());
    return *this;
}

// Pipe stdin

Process &   Process::stdin_close()
{
    this->_fdw_close(_stdin);
    return *this;
}

Process &   Process::stdin_from(const std::string & input)
{
    this->_add_pipe(_stdin);
    this->_write_into_fd(_stdin.fd_write, input);
    return *this;
}

Process &   Process::stdin_from(int fd)
{
    _stdin.fd_read = fd;
    return *this;
}

bool   Process::stdin_from_file(const std::string & path)
{
    _stdin.fd_read = open(path.c_str(), O_RDONLY);
    if (_stdin.fd_read < 0)
        LOG(error, "Process: could not open file input: " << path);
    return _stdin.fd_read >= 0;
}

// Pipe stdout

Process &   Process::stdout_close()
{
    this->_fdw_close(_stdout);
    return *this;
}

Process &   Process::stdout_to(std::function<void(const char *, ssize_t)> fun)
{
    this->_fdw_to(_stdout, std::move(fun));
    return *this;
}

Process &   Process::stdout_to(std::string & output)
{
    this->_fdw_to(_stdout, output);
    return *this;
}

Process &   Process::stdout_to(int fd)
{
    this->_fdw_to(_stdout, fd);
    return *this;
}

Process &   Process::stdout_to(Process & proc)
{
    this->_add_pipe(_stdout);
    proc.stdin_from(_stdout.fd_read);
    _stdout.fd_read = -1;
    return *this;
}

bool   Process::stdout_to_file(const std::string & path, bool append)
{
    return this->_fdw_to_file(_stdout, path, append);
}

// Pipe stderr

Process &   Process::stderr_close()
{
    this->_fdw_close(_stderr);
    return *this;
}

Process &   Process::stderr_to(std::function<void(const char *, ssize_t)> fun)
{
    this->_fdw_to(_stderr, std::move(fun));
    return *this;
}

Process &   Process::stderr_to(std::string & output)
{
    this->_fdw_to(_stderr, output);
    return *this;
}

Process &   Process::stderr_to(int fd)
{
    this->_fdw_to(_stderr, fd);
    return *this;
}

Process &   Process::stderr_to(Process & proc)
{
    this->_add_pipe(_stderr);
    proc.stdin_from(_stderr.fd_read);
    _stderr.fd_read = -1;
    return *this;
}

bool   Process::stderr_to_file(const std::string & path, bool append)
{
    return this->_fdw_to_file(_stderr, path, append);
}


// Private fdw setters

void    Process::_fdw_close(FileDescWrapper & fdw)
{
    fdw.action = CLOSE;
    this->_close(fdw.fd_read);
    this->_close(fdw.fd_write);
    fdw.fd_read = -1;
    fdw.fd_write = -1;
}

void    Process::_fdw_to(FileDescWrapper & fdw, std::function<void(const char *, ssize_t)> && fun)
{
    this->_add_pipe(fdw);
    fdw.fun = std::move(fun);
}

void    Process::_fdw_to(FileDescWrapper & fdw, std::string & output)
{
    this->_add_pipe(fdw);
    fdw.str_out = output;
}

void    Process::_fdw_to(FileDescWrapper & fdw, int fd)
{
    fdw.fd_write = fd;
}

bool    Process::_fdw_to_file(FileDescWrapper & fdw, const std::string & path, bool append)
{
    fdw.fd_write = open(path.c_str(),
                        O_WRONLY | O_CREAT | (append ? O_APPEND : 0),
                        this->open_mode);
    if (fdw.fd_write >= 0)
        fdw.action = append ? FILE_APPEND : FILE;
    else
        LOG(error, "Process: could not open output file: " << path);
    return fdw.fd_write >= 0;
}

// Private stuff

bool Process::_write_into_fd(int fd, const std::string & str)
{
    ssize_t ret = write(fd, str.c_str(), str.size());
    return ret >= 0 && ret == (ssize_t)str.size();
}

void    Process::_dup_close(int fd_from, int fd_to)
{
    if (fd_from >= 0)
    {
        if (dup2(fd_from, fd_to) == -1)
        {
            LOG(error, "Process: could not duplicate fd: " << strerror(errno));
        }
        this->_close(fd_from);
    }
}

void    Process::_close(int fd)
{
    if (fd == -1)
        return ;
    if (close(fd) == -1)
        LOG(error, "Process: could not close fd: " << strerror(errno));
}

std::pair<int, int> Process::_pipe()
{
    int fd[2];

    if (pipe(fd) < 0)
        throw std::runtime_error("Cannot make pipe");
    // read - write;
    return std::make_pair(fd[0], fd[1]);
}

void    Process::_add_pipe(FileDescWrapper & fdw)
{
    if (fdw.fd_write == -1)
    {
        std::pair<int, int> pipe = this->_pipe();

        fdw.fd_read = pipe.first;
        fdw.fd_write = pipe.second;
    }
}

// Execution

int    Process::_exec_child()
{
    if (_fun_to_execute)
        return _fun_to_execute();
    return execvp(_argv[0], const_cast<char * const *>(&(_argv[0])));
}

void    Process::_do_fork()
{
    pid_t pid;
    if ((pid = fork()) < 0)
        throw std::runtime_error("Fork failed");
    if (pid == 0)
    {
        if (_stdin.action == CLOSE)
            this->_close(STDIN_FILENO);
        else
        {
            this->_dup_close(_stdin.fd_read, STDIN_FILENO);
            this->_close(_stdin.fd_write);
        }
        if (_stdout.action == CLOSE)
            this->_close(STDOUT_FILENO);
        else
        {
            this->_dup_close(_stdout.fd_write, STDOUT_FILENO);
            this->_close(_stdout.fd_read);
        }
        if (_stderr.action == CLOSE)
            this->_close(STDERR_FILENO);
        else
        {
            this->_dup_close(_stderr.fd_write, STDERR_FILENO);
            this->_close(_stderr.fd_read);
        }
        exit(this->_exec_child());
    }
    _pid = pid;
}

#if !defined(__SIHD_ANDROID__)
void    Process::_add_dup_action(posix_spawn_file_actions_t *actions, int dup_from, int dup_to)
{
    if (dup_from >= 0 && dup_to >= 0)
    {
        posix_spawn_file_actions_adddup2(actions, dup_from, dup_to);
        posix_spawn_file_actions_addclose(actions, dup_from);
    }
}

void    Process::_add_close_action(posix_spawn_file_actions_t *actions, int fd)
{
    if (fd >= 0)
        posix_spawn_file_actions_addclose(actions, fd);
}

bool    Process::_do_spawn()
{
    pid_t pid;
    posix_spawn_file_actions_t actions;

    posix_spawn_file_actions_init(&actions);
    if (_stdin.action == CLOSE)
        this->_add_close_action(&actions, STDIN_FILENO);
    else
    {
        this->_add_dup_action(&actions, _stdin.fd_read, STDIN_FILENO);
        this->_add_close_action(&actions, _stdin.fd_write);
    }
    if (_stdout.action == CLOSE)
        this->_add_close_action(&actions, STDOUT_FILENO);
    else
    {
        this->_add_dup_action(&actions, _stdout.fd_write, STDOUT_FILENO);
        this->_add_close_action(&actions, _stdout.fd_read);
    }
    if (_stderr.action == CLOSE)
        this->_add_close_action(&actions, STDERR_FILENO);
    else
    {
        this->_add_dup_action(&actions, _stderr.fd_write, STDERR_FILENO);
        this->_add_close_action(&actions, _stderr.fd_read);
    }
    int err = posix_spawnp(&pid, _argv[0], &actions, nullptr,
                            const_cast<char * const *>(&(_argv[0])),
                            nullptr);
    posix_spawn_file_actions_destroy(&actions);
    if (err != 0)
    {
        LOG(error, "Process: " << strerror(errno));
        return false;
    }
    _pid = pid;
    return true;
}
#endif

bool    Process::run()
{
    if (!_fun_to_execute)
    {
        if (_argv.size() == 0)
        {
            LOG(error, "Process: Could not run process, no argv");
            return false;
        }
        if (_argv.back() != NULL)
            _argv.push_back(NULL);
#if !defined(__SIHD_ANDROID__)
        if (this->_do_spawn() == false)
            return false;
#else
        this->_do_fork();
#endif
    }
    else
        this->_do_fork();
    this->_close(_stdin.fd_read);
    this->_close(_stdout.fd_write);
    this->_close(_stderr.fd_write);
    _stdin.fd_read = -1;
    _stdout.fd_write = -1;
    _stderr.fd_write = -1;
    return true;
}

bool    Process::_read_fd(int fd, std::function<void(const char *, ssize_t)> fun)
{
    char buffer[SIHD_PROCESS_READ_BUFFER_SIZE];
    ssize_t ret;

    while ((ret = read(fd, &buffer, SIHD_PROCESS_READ_BUFFER_SIZE)) > 0)
    {
        buffer[ret] = 0;
        fun(buffer, ret);
    }
    return ret != -1;
}

bool    Process::_write_into_file(int fd, std::string & path, bool append)
{
    std::ofstream file(path, append ? std::ostream::app : std::ostream::out);

    auto fun = [&file] (const char *buffer, [[maybe_unused]] ssize_t size)
    {
        file << buffer;
    };
    if (!file.is_open() || !file.good())
        return false;
    return this->_read_fd(fd, fun);
}

bool    Process::_process_fd_out(FileDescWrapper & fdw)
{
    if (fdw.fd_read < 0)
        return true;
    if (fdw.action == FILE || fdw.action == FILE_APPEND)
        return this->_write_into_file(fdw.fd_read, fdw.str_out.value().get(), fdw.action == FILE_APPEND);
    else if (fdw.str_out.has_value())
    {
        std::string & out = fdw.str_out.value().get();
        return this->_read_fd(fdw.fd_read, [&out] (const char *buffer, [[maybe_unused]] ssize_t ret) { out.append(buffer); });
    }
    else if (fdw.fun)
        return this->_read_fd(fdw.fd_read, fdw.fun);
    return true;
}

bool    Process::process()
{
    // may be stuck if stdin is still open
    if (_stdin.fd_write >= 0)
    {
        LOG(warning, "Process: trying to process stdout/stderr with stdin still open");
        return false;
    }
    bool ret = this->has_run();
    if (ret)
    {
        ret = ret && this->_process_fd_out(_stdout);
        ret = ret && this->_process_fd_out(_stderr);
    }
    return ret;
}

bool    Process::end()
{
    bool ret = this->has_run();
    if (ret)
    {
        this->_close(_stdin.fd_write);
        _stdin.fd_write = -1;
        ret = this->process();
        this->_close(_stdout.fd_read);
        this->_close(_stderr.fd_read);
        _stdout.fd_read = -1;
        _stderr.fd_read = -1;
        this->wait();
    }
    return ret;
}

bool    Process::kill(int sig)
{
    bool ret = this->has_run();
    if (ret)
    {
        ret = ::kill(this->pid(), sig) >= 0;
        if (!ret)
            LOG(error, "Process: could not kill: " << strerror(errno));
    }
    return ret;
}

// Check process

bool    Process::has_run()
{
    return _pid >= 0;
}

std::optional<int>  Process::wait(int options)
{
    if (this->has_run())
    {
        int st;
        waitpid(_pid, &st, options);
        _status = st;
        return st;
    }
    return std::nullopt;
}

std::optional<bool>    Process::has_exited()
{
    return _status ? std::optional<bool>{WIFEXITED(_status.value())} : std::nullopt;
}

std::optional<bool>    Process::has_core_dumped()
{
    return _status ? std::optional<bool>{WCOREDUMP(_status.value())} : std::nullopt;
}

std::optional<bool>    Process::has_stopped_by_signal()
{
    return _status ? std::optional<bool>{WIFSTOPPED(_status.value())} : std::nullopt;
}

std::optional<bool>    Process::has_exited_by_signal()
{
    return _status ? std::optional<bool>{WIFSIGNALED(_status.value())} : std::nullopt;
}

std::optional<bool>  Process::has_continued()
{
    return _status ? std::optional<bool>{WIFCONTINUED(_status.value())} : std::nullopt;
}

std::optional<int>  Process::signal_exit_number()
{
    return _status ? std::optional<int>{WTERMSIG(_status.value())} : std::nullopt;
}

std::optional<int>  Process::signal_stop_number()
{
    return _status ? std::optional<int>{WSTOPSIG(_status.value())} : std::nullopt;
}

std::optional<int>  Process::return_code()
{
    return _status ? std::optional<int>{WEXITSTATUS(_status.value())} : std::nullopt;
}


}
#endif