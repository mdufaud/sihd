#ifndef __SIHD_SSH_SSHSUBSYSTEMEXEC_HPP__
#define __SIHD_SSH_SSHSUBSYSTEMEXEC_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>

#include <string>
#include <vector>

#include <sihd/sys/Process.hpp>

namespace sihd::ssh
{

/**
 * SSH exec subsystem - executes system commands.
 *
 * Modes:
 * - Fork (default): spawn child process, async, forward output via forward_output()
 * - Synchronous: runs to completion in on_start(), sends output and closes
 *
 * Parse modes:
 * - Shell (default): "/bin/sh -c command" (or "cmd.exe /c command" on Windows)
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
        bool on_start(SshChannel *channel, bool has_pty, const WinSize & winsize) override;
        int on_data(const void *data, size_t len) override;
        void on_resize(const WinSize & winsize) override;
        void on_eof() override;
        int on_close() override;

        int stdout_fd() const override { return -1; }
        int stderr_fd() const override { return -1; }
        SshChannel *channel() const override { return _channel; }
        bool is_running() const override;
        bool forward_output() override;

    private:
        bool start_process_mode();
        bool start_sync_mode();

        SshChannel *_channel;
        std::string _command;
        std::string _shell;
        std::string _working_dir;
        ParseMode _parse_mode;
        bool _fork_mode;
        bool _started;
        int _exit_code;
        bool _has_pty;
        WinSize _winsize;

        std::vector<std::pair<std::string, std::string>> _env;

        // Process-based execution (fork mode)
        sihd::sys::Process _process;
};

} // namespace sihd::ssh

#endif
