#include <sihd/sys/Daemon.hpp>
#include <sihd/sys/NamedFactory.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/signal.hpp>
#include <sihd/util/Logger.hpp>

// run(), set_user()/set_group() account resolution live in src/linux|windows/Daemon.cpp

namespace sihd::sys
{

using namespace sihd::util;

SIHD_REGISTER_FACTORY(Daemon)

SIHD_LOGGER;

Daemon::Daemon(const std::string & name, sihd::util::Node *parent): sihd::util::Named(name, parent)
{
    _signals_handled = false;
#if !defined(__SIHD_WINDOWS__)
    _working_dir_path = "/";
    _pid_file_path = fmt::format("/var/lock/{}_daemon.lock", this->name());
#else
    _working_dir_path = fs::home_path();
#endif
    this->add_conf("user", static_cast<bool (Daemon::*)(std::string_view)>(&Daemon::set_user));
    this->add_conf("group", static_cast<bool (Daemon::*)(std::string_view)>(&Daemon::set_group));
    this->add_conf("pid_file", &Daemon::set_pid_file_path);
    this->add_conf("working_dir", &Daemon::set_working_dir_path);
}

Daemon::~Daemon()
{
    this->_remove_pid_file();
}

bool Daemon::_set_drop_target(const user::UserId & user_id)
{
    if (!user_id.valid())
    {
        SIHD_LOG(error, "Daemon: cannot drop to an invalid user");
        return false;
    }
    _user = user_id;
    if (_group_explicit)
        return true;
    const auto group_id = user::primary_group_of(user_id);
    if (!group_id.has_value())
    {
        SIHD_LOG(error, "Daemon: no primary group for user {}", user_id.to_string());
        return false;
    }
    _group = *group_id;
    return true;
}

bool Daemon::_set_group(const user::GroupId & group_id)
{
    if (!group_id.valid())
    {
        SIHD_LOG(error, "Daemon: cannot drop to an invalid group");
        return false;
    }
    _group = group_id;
    _group_explicit = true;
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

    std::string towrite = str::to_dec(os::pid()) + "\n";
    bool ret = file.write(towrite) == (ssize_t)towrite.size();
    if (ret == false)
        SIHD_LOG(error, "Daemon: failed to write pid file");

    return ret;
}

} // namespace sihd::sys
