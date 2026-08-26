#include <fcntl.h>    // open, O_RDWR
#include <sys/stat.h> // umask
#include <unistd.h>   // fork, setsid, chdir, chown, dup2

#include <sihd/sys/Daemon.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::sys
{

using namespace sihd::util;

SIHD_LOGGER;

#if !defined(__SIHD_EMSCRIPTEN__)

bool Daemon::set_user(std::string_view user_name)
{
    const auto user_id = user::user_id_of(user_name);
    if (!user_id.has_value())
    {
        SIHD_LOG(error, "Daemon: cannot resolve user '{}'", user_name);
        return false;
    }
    return this->_set_drop_target(*user_id);
}

bool Daemon::set_user(const user::UserId & user_id)
{
    return this->_set_drop_target(user_id);
}

bool Daemon::set_group(std::string_view group_name)
{
    const auto group_id = user::group_id_of(group_name);
    if (!group_id.has_value())
    {
        SIHD_LOG(error, "Daemon: cannot resolve group '{}'", group_name);
        return false;
    }
    return this->_set_group(*group_id);
}

bool Daemon::set_group(const user::GroupId & group_id)
{
    return this->_set_group(group_id);
}

bool Daemon::run()
{
    // lock file
    if (_lock_pid_file() == false)
        return false;
    // first fork (run in background)
    pid_t pid = fork();
    if (pid < 0)
    {
        SIHD_LOG(error, "Daemon: fork failed: {}", os::last_error_str());
        return false;
    }
    else if (pid > 0)
        _exit(EXIT_SUCCESS);
    // install signal handlers
    this->_handle_signals();
    // process not killed once shell is exited
    pid_t sid = setsid();
    if (sid < 0)
    {
        SIHD_LOG(error, "Daemon: setsid failed: {}", os::last_error_str());
        _exit(1);
    }
    // second fork (cannot take a controlling terminal)
    if ((pid = fork()) < 0)
    {
        SIHD_LOG(error, "Daemon: second fork failed: {}", os::last_error_str());
        _exit(2);
    }
    if (pid > 0)
        _exit(EXIT_SUCCESS);
    // write pid file
    _write_pid_file();
    // change file creation mask
    umask(0);
    // change directory
    if (chdir(_working_dir_path.c_str()) < 0)
    {
        SIHD_LOG(error, "Daemon: chdir failed: {}", os::last_error_str());
        _exit(3);
    }
    // drop to the requested account
    if (_user.valid())
    {
        // set right ownership to pid file
        if (chown(_pid_file_path.c_str(), _user.native(), _group.native()) != 0)
        {
            SIHD_LOG(warning, "Daemon: can't set pid file ownership to uid: {}", _user.to_string());
        }
        // a daemon that keeps running as root after a requested drop is a privilege escalation
        if (user::drop_privileges(_user, _group) == false)
        {
            SIHD_LOG(error, "Daemon: cannot drop privileges to uid {}", _user.to_string());
            _exit(4);
        }
    }
    // redirect standard file descriptors to /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    if (open("/dev/null", O_RDWR) == STDIN_FILENO)
    {
        if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            SIHD_LOG(warning, "Daemon: could not redirect stdout to /dev/null");
        if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            SIHD_LOG(warning, "Daemon: could not redirect stderr to /dev/null");
    }
    else
    {
        SIHD_LOG(warning, "Daemon: could not redirect stdin to /dev/null");
    }
    SIHD_LOG(info, "Daemon: started with pid: {}", getpid());
    return true;
}

#else

bool Daemon::set_user(std::string_view)
{
    SIHD_LOG(error, "Daemon: this platform cannot drop the process to another account");
    return false;
}

bool Daemon::set_user(const user::UserId &)
{
    SIHD_LOG(error, "Daemon: this platform cannot drop the process to another account");
    return false;
}

bool Daemon::set_group(std::string_view)
{
    SIHD_LOG(error, "Daemon: this platform cannot drop the process to another account");
    return false;
}

bool Daemon::set_group(const user::GroupId &)
{
    SIHD_LOG(error, "Daemon: this platform cannot drop the process to another account");
    return false;
}

bool Daemon::run()
{
    SIHD_LOG(error, "Daemon: not supported on this platform");
    return false;
}

#endif

} // namespace sihd::sys
