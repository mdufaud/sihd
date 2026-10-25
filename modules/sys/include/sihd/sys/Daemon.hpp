#ifndef __SIHD_SYS_DAEMON_HPP__
#define __SIHD_SYS_DAEMON_HPP__

#include <mutex>

#include <sihd/sys/FileMutex.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

class Daemon: public sihd::util::Named,
              public sihd::util::Configurable,
              public sihd::util::IRunnable
{
    public:
        // daemonize (fork/setsid) only on unix; no-op on windows and emscripten
        static constexpr bool supported = !sihd::util::build::is_windows && !sihd::util::build::is_emscripten;

        Daemon(const std::string & name, sihd::util::Node *parent = nullptr);
        ~Daemon();

        // drop target after daemonize: the user's PRIMARY group unless set_group() overrides it
        bool set_user(const std::string & user_name);
        bool set_user(const user::UserId & user_id);
        bool set_group(const std::string & group_name);
        bool set_group(const user::GroupId & group_id);
        bool set_pid_file_path(std::string_view path);
        bool set_working_dir_path(std::string_view path);

        /*
            clear LoggerManager's loggers and/or install any file logger before calling run
        */
        bool run();

        const user::UserId & user_id() const { return _user; }
        const user::GroupId & group_id() const { return _group; }
        const std::string & pid_file() const { return _pid_file_path; }
        const std::string & working_dir() const { return _working_dir_path; }

    private:
        bool _lock_pid_file();
        bool _write_pid_file();
        void _remove_pid_file();
        bool _handle_signals();

        bool _set_drop_target(const user::UserId & user_id);
        bool _set_group(const user::GroupId & group_id);

        bool _signals_handled;
        user::UserId _user;
        user::GroupId _group;
        // true when set_group() set the group, so _set_drop_target must not overwrite it
        bool _group_explicit = false;
        std::string _pid_file_path;
        std::string _working_dir_path;
        FileMutex _pid_file_mutex;
        std::unique_lock<FileMutex> _lock;
};

} // namespace sihd::sys

#endif