#ifndef __SIHD_SSH_SSHSUBSYSTEMEXEC_HPP__
#define __SIHD_SSH_SSHSUBSYSTEMEXEC_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>

#include <string>
#include <sys/types.h>
#include <vector>

namespace sihd::ssh
{

/**
 * SSH exec subsystem - executes system commands.
 *
 * Modes:
 * - Fork (default): fork+exec, async, returns FDs to poll
 * - Synchronous: runs to completion in on_start(), sends output and closes
 *
 * Parse modes:
 * - Shell (default): "/bin/sh -c command"
 * - Direct: splits command and runs without shell
 */
class SshSubsystemExec: public ISshSubsystemHandler
{
    public:
        enum class ParseMode
        {
            Shell,
            Direct
        };

        explicit SshSubsystemExec(std::string_view command);
        ~SshSubsystemExec() override;

        // Configuration (call before on_start)
        void set_shell(std::string_view shell);
        void set_fork_mode(bool enable);
        void set_parse_mode(ParseMode mode);
        void set_env(std::string_view name, std::string_view value);
        void set_working_directory(std::string_view path);

        const std::string & command() const { return _command; }

        // ISshSubsystemHandler
        bool on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize) override;
        int on_data(const void *data, size_t len) override;
        void on_resize(const struct winsize & winsize) override;
        void on_eof() override;
        int on_close() override;

        int stdout_fd() const override { return _stdout_fd; }
        int stderr_fd() const override { return _stderr_fd; }
        SshChannel *channel() const override { return _channel; }
        bool is_running() const override;

    private:
        bool start_fork_mode();
        bool start_sync_mode();
        void close_fds();

        SshChannel *_channel;
        std::string _command;
        std::string _shell;
        std::string _working_dir;
        ParseMode _parse_mode;
        bool _fork_mode;
        bool _started;
        int _exit_code;

        pid_t _pid;
        int _stdout_fd;
        int _stderr_fd;
        int _stdin_fd;
        bool _has_pty;
        struct winsize _winsize;

        std::vector<std::pair<std::string, std::string>> _env;
};

} // namespace sihd::ssh

#endif
