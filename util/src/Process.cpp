#include <sihd/util/Process.hpp>
		
#if !defined(__SIHD_WINDOWS__)

# include <stdexcept>
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>

namespace sihd::util
{

Process::Process(const std::vector<std::string> & cmd)
{
    this->_init();
    _cmd.reserve(cmd.size() + 1);
    for (const std::string & arg: cmd)
        _cmd.push_back(arg.c_str());
    _cmd.push_back(NULL);
}

Process::Process(std::initializer_list<const char *> cmd)
{
    this->_init();
    _cmd.reserve(cmd.size() + 1);
    std::copy(cmd.begin(), cmd.end(), std::back_inserter(_cmd));
    _cmd.push_back(NULL);
}

Process::~Process()
{
}


void    Process::_init()
{
    _pid = -1;
    _stdin_fd = -1;
    _stdout_fd = -1;
    _stderr_fd = -1;
}


void    Process::_dup_close(int fd_from, int fd_to)
{
    if (fd_from >= 0)
    {
        dup2(fd_from, fd_to);
        close(fd_from);
    }
}

void    Process::_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

std::pair<int, int> Process::_pipe()
{
    int fd[2];
    if (pipe(fd) < 0)
        throw std::runtime_error("Cannot make pipe");
    // read - write;
    return std::make_pair(fd[0], fd[1]);
}

bool    Process::run()
{
    int pid;

    if ((pid = fork()) < 0)
        return false;
    if (pid == 0)
    {
        this->_dup_close(_stdin_fd, STDIN_FILENO);
        this->_dup_close(_stdout_fd, STDOUT_FILENO);
        this->_dup_close(_stderr_fd, STDERR_FILENO);
        exit(execvp(_cmd[0], const_cast<char * const *>(&(_cmd[0]))));
    }
    this->_close(_stdin_fd);
    this->_close(_stdout_fd);
    this->_close(_stderr_fd);
    _pid = pid;
    return true;
}

bool     Process::has_run()
{
    return _pid >= 0;
}

std::optional<int>  Process::wait()
{
    if (this->has_run())
    {
        int st;
        waitpid(_pid, &st, 0);
        _status = st;
        return st;
    }
    return std::nullopt;
}

std::optional<bool>    Process::has_stopped()
{
    return _status ? std::optional<bool>{WIFSTOPPED(_status.value())} : std::nullopt;
}

std::optional<bool>    Process::has_exited()
{
    return _status ? std::optional<bool>{WIFEXITED(_status.value())} : std::nullopt;
}

std::optional<bool>    Process::has_core_dumped()
{
    return _status ? std::optional<bool>{WCOREDUMP(_status.value())} : std::nullopt;
}

std::optional<bool>    Process::has_exited_by_signal()
{
    return _status ? std::optional<bool>{WIFSIGNALED(_status.value())} : std::nullopt;
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