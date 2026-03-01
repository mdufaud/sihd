#ifndef __SIHD_SYS_DAEMON_HPP__
#define __SIHD_SYS_DAEMON_HPP__

#include <mutex>

#include <sihd/sys/FileMutex.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/IRunnable.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::sys
{

class Daemon: public sihd::util::Named,
              public sihd::util::Configurable,
              public sihd::util::IRunnable
{
    public:
        Daemon(const std::string & name, sihd::util::Node *parent = nullptr);
        ~Daemon();

        bool set_uid(uid_t uid);
        bool set_pid_file_path(std::string_view path);
        bool set_working_dir_path(std::string_view path);

        /*
            clear LoggerManager's loggers and/or install any file logger before calling run
        */
        bool run();

        uid_t uid() const { return _uid; }
        const std::string & pid_file() const { return _pid_file_path; }
        const std::string & working_dir() const { return _working_dir_path; }

    private:
        bool _lock_pid_file();
        bool _write_pid_file();
        void _remove_pid_file();
        bool _handle_signals();

        bool _signals_handled;
        uid_t _uid;
        std::string _pid_file_path;
        std::string _working_dir_path;
        FileMutex _pid_file_mutex;
        std::unique_lock<FileMutex> _lock;
};

} // namespace sihd::sys

#endif