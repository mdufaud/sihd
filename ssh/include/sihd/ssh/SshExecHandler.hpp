#ifndef __SIHD_SSH_SSHEXECHANDLER_HPP__
#define __SIHD_SSH_SSHEXECHANDLER_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>

#include <functional>
#include <string>
#include <sys/types.h>

namespace sihd::ssh
{

/**
 * Exec command handler.
 *
 * Handles single command execution (ssh user@host "command").
 *
 * Modes:
 * 1. Callback mode: set_exec_callback() for in-process handling
 * 2. Fork mode: set_fork_mode(true) to fork and exec
 *
 * Parse modes (for non-callback execution):
 * - Shell: runs "/bin/sh -c command" (default)
 * - Direct: parses command with Splitter and runs directly via proc::execute
 *
 * Usage (callback mode):
 *   auto handler = new SshExecHandler("ls -la");
 *   handler->set_exec_callback([](const std::string& cmd) {
 *       return ExecResult{"output", 0};
 *   });
 *
 * Usage (shell mode - default):
 *   auto handler = new SshExecHandler("ls -la");
 *   // Uses /bin/sh -c "ls -la"
 *
 * Usage (direct mode):
 *   auto handler = new SshExecHandler("ls -la file.txt");
 *   handler->set_parse_mode(ParseMode::Direct);
 *   // Parses command and runs ["ls", "-la", "file.txt"] directly
 */
class SshExecHandler: public ISshSubsystemHandler
{
    public:
        struct ExecResult
        {
                std::string output;
                int exit_code;
        };

        /**
         * How to parse and execute the command.
         */
        enum class ParseMode
        {
            Shell, ///< Run via shell: /bin/sh -c "command" (default)
            Direct ///< Parse with Splitter and run directly via proc::execute
        };

        /**
         * Callback for command execution.
         * @param command The command to execute
         * @return ExecResult with output and exit code
         */
        using ExecCallback = std::function<ExecResult(const std::string & command)>;

        explicit SshExecHandler(std::string_view command);
        ~SshExecHandler() override;

        // Configuration
        void set_exec_callback(ExecCallback callback);
        void set_shell(std::string_view shell);
        void set_fork_mode(bool enable);
        void set_parse_mode(ParseMode mode);

        // Command accessors
        const std::string & command() const { return _command; }

        // ISshSubsystemHandler interface
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
        bool start_callback_mode();
        void close_fds();

        SshChannel *_channel;
        std::string _command;
        std::string _shell;
        ParseMode _parse_mode;
        bool _fork_mode;
        bool _started;
        int _exit_code;

        // Fork mode state
        pid_t _pid;
        int _stdout_fd;
        int _stderr_fd;
        int _stdin_fd;
        bool _has_pty;
        struct winsize _winsize;

        ExecCallback _exec_callback;
};

} // namespace sihd::ssh

#endif
