#include <sys/stat.h> //umask
#include <unistd.h>

#include <csignal>

#include <sihd/util/Daemon.hpp>
#include <sihd/util/FileLogger.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/NamedFactory.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/signal.hpp>

#if defined(__SIHD_WINDOWS__)
# include <windows.h>
#endif

namespace sihd::util
{

SIHD_UTIL_REGISTER_FACTORY(Daemon)

SIHD_LOGGER;

Daemon::Daemon(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent)
{
    _signals_handled = false;
    _uid = 0;
#if !defined(__SIHD_WINDOWS__)
    _working_dir_path = fs::sep_str();
    _pid_file_path = "/var/lock/" + this->name() + "_daemon.lock";
#else
    _working_dir_path = fs::home_path();
#endif
    this->add_conf("uid", &Daemon::set_uid);
    this->add_conf("pid_file", &Daemon::set_pid_file_path);
    this->add_conf("working_dir", &Daemon::set_working_dir_path);
}

Daemon::~Daemon()
{
    this->_remove_pid_file();
}

bool Daemon::set_uid(uid_t uid)
{
    _uid = uid;
    return true;
}

bool Daemon::set_pid_file_path(std::string_view path)
{
    _pid_file_path = path;
    return true;
}

bool Daemon::set_working_dir_path(std::string_view path)
{
    _working_dir_path = path;
    return true;
}

bool Daemon::_handle_signals()
{
    if (_signals_handled)
        return true;
    bool ret = true;
    int sig = 1;
    while (sig < 65)
    {
        if (signal::handle(sig) == false)
            ret = false;
        ++sig;
    }
    signal::set_exit_config({.on_stop = false,
                             .on_termination = false,
                             .on_dump = true,
                             .log_signal = true,
                             .exit_with_sig_number = false});
    _signals_handled = true;
    return ret;
}

void Daemon::_remove_pid_file()
{
    File & file = _pid_file_mutex.file();
    if (file.is_open())
    {
        std::string path = file.path();
        file.close();
        fs::remove_file(path);
    }
}

bool Daemon::_lock_pid_file()
{
    if (_lock.owns_lock())
        return true;

    FileMutex tmp(_pid_file_path, true);
    _pid_file_mutex = std::move(tmp);

    _lock = std::unique_lock(_pid_file_mutex);
    if (_lock.try_lock() == false)
    {
        SIHD_LOG(error, "Daemon: cannot lock file");
        return false;
    }
    return true;
}

bool Daemon::_write_pid_file()
{
    File & file = _pid_file_mutex.file();

    if (file.is_open() == false)
        return false;

    std::string towrite = str::to_dec(getpid()) + "\n";
    bool ret = file.write(towrite) == (ssize_t)towrite.size();
    if (ret == false)
        SIHD_LOG(error, "Daemon: failed to write pid file");

    return ret;
}

#if !defined(__SIHD_WINDOWS__)

bool Daemon::run()
{
    // lock file
    if (_lock_pid_file() == false)
        return false;
    // first fork (run in background)
    pid_t pid = fork();
    if (pid < 0)
    {
        SIHD_LOG(error, "Daemon: fork failed: {}", strerror(errno));
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
        SIHD_LOG(error, "Daemon: setsid failed: {}", strerror(errno));
        _exit(1);
    }
    // second fork (cannot take a controlling terminal)
    if ((pid = fork()) < 0)
    {
        SIHD_LOG(error, "Daemon: second fork failed: {}", strerror(errno));
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
        SIHD_LOG(error, "Daemon: chdir failed: {}", strerror(errno));
        _exit(3);
    }
    // change uid
    if (_uid)
    {
        // set right ownership to pid file
        if (chown(_pid_file_path.c_str(), _uid, 0) != 0)
        {
            SIHD_LOG(warning, "Daemon: can't set pid file ownership to uid: {}", _uid);
        }
        if (setuid(_uid) != 0)
        {
            SIHD_LOG(warning, "Daemon: can't set process ownership to uid: {}", _uid);
        }
    }
    // close file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    /*
    if (open("/dev/null", O_RDWR) == STDIN_FILENO)
    {
        if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            SIHD_LOG(warning, "Daemon: could not open back stdout file number");
        if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            SIHD_LOG(warning, "Daemon: could not open back stderr file number");
    }
    else
    {
        SIHD_LOG(warning, "Daemon: could not open back stdin file number");
    }
    */
    SIHD_LOG(info, "Daemon: started with pid: {}", getpid());
    return true;
}

#else

# pragma message("TODO windows")

bool Daemon::run()
{
    // TODO
    // maybe use windows services
    FreeConsole();
    if (chdir(_working_dir_path.c_str()) < 0)
    {
        SIHD_LOG(error, "Daemon: chdir failed: {}", strerror(errno));
        return false;
    }
    return true;
}

#endif

} // namespace sihd::util
