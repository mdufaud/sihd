#ifndef __SIHD_SSH_SSHPTYHANDLER_HPP__
#define __SIHD_SSH_SSHPTYHANDLER_HPP__

#include <sihd/ssh/ISshSubsystemHandler.hpp>
#include <sihd/ssh/Pty.hpp>

#include <memory>
#include <string>
#include <vector>

namespace sihd::ssh
{

/**
 * @brief SSH PTY handler using the cross-platform Pty abstraction.
 *
 * This handler spawns a real shell in a pseudo-terminal and bridges it
 * to an SSH channel. It provides full terminal functionality including:
 * - Line editing (readline, history)
 * - Job control (Ctrl+C, Ctrl+Z)
 * - Terminal resize support
 * - Proper terminal escape sequences
 *
 * ## Cross-Platform Support
 * - **Linux/macOS**: Uses PosixPty (forkpty)
 * - **Windows 10 1809+**: Uses ConPty (CreatePseudoConsole)
 * - **Older Windows**: Not supported (returns false from on_start)
 *
 * ## Usage
 * ```cpp
 * // In your SSH server handler:
 * ISshSubsystemHandler *on_shell_request(...) override {
 *     auto *handler = new SshPtyHandler();
 *     handler->set_shell("/bin/bash");
 *     handler->set_env("LANG", "en_US.UTF-8");
 *     return handler;
 * }
 * ```
 *
 * ## Data Flow
 * ```
 * [SSH Client] <--> [SshChannel] <--> [SshPtyHandler] <--> [Pty] <--> [Shell]
 * ```
 *
 * The handler's stdout_fd() returns the PTY read descriptor for polling.
 * The SSH server should poll this fd and call channel->write() when data
 * is available.
 *
 * @see Pty, ISshSubsystemHandler
 */
class SshPtyHandler: public ISshSubsystemHandler
{
    public:
        SshPtyHandler();
        ~SshPtyHandler() override;

        /**
         * @brief Check if PTY is supported on this platform.
         *
         * @return true if PTY can be created
         * @return false if PTY is not supported (e.g., old Windows)
         */
        static bool is_supported();

        // ===== Configuration (call before on_start) =====

        /**
         * @brief Set the shell to run.
         *
         * @param shell Path to shell executable
         *
         * Defaults:
         * - POSIX: "/bin/sh"
         * - Windows: "cmd.exe"
         */
        void set_shell(std::string_view shell);

        /**
         * @brief Set shell arguments.
         *
         * @param args Command-line arguments for the shell
         *
         * Example: {"-l"} for login shell on POSIX
         */
        void set_args(std::vector<std::string> args);

        /**
         * @brief Set an environment variable for the shell.
         *
         * @param name Variable name
         * @param value Variable value
         *
         * Common variables:
         * - TERM: Usually set automatically from SSH client
         * - LANG: Locale
         */
        void set_env(std::string_view name, std::string_view value);

        /**
         * @brief Set the working directory for the shell.
         *
         * @param path Directory path
         */
        void set_working_directory(std::string_view path);

        // ===== ISshSubsystemHandler interface =====

        /**
         * @brief Start the PTY shell.
         *
         * @param channel SSH channel for communication
         * @param has_pty Whether the client requested a PTY
         * @param winsize Terminal dimensions from the client
         * @return true on success
         * @return false if PTY creation or spawn failed
         */
        bool on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize) override;

        /**
         * @brief Handle data from SSH client.
         *
         * Data is written directly to the PTY input.
         *
         * @param data Input data (keystrokes from client)
         * @param len Data length
         * @return Number of bytes processed
         */
        int on_data(const void *data, size_t len) override;

        /**
         * @brief Handle terminal resize from SSH client.
         *
         * @param winsize New terminal dimensions
         */
        void on_resize(const struct winsize & winsize) override;

        /**
         * @brief Handle EOF from SSH client.
         *
         * Signals EOF to the shell process.
         */
        void on_eof() override;

        /**
         * @brief Handle channel close.
         *
         * Terminates the shell process and waits for exit.
         *
         * @return Shell exit code
         */
        int on_close() override;

        /**
         * @brief Get file descriptor for polling shell output.
         *
         * @return PTY read fd, or -1 if not running
         *
         * Note: On Windows, this returns a placeholder. The actual HANDLE
         * must be used with WaitForSingleObject, not poll/select.
         */
        int stdout_fd() const override;

        /**
         * @brief No separate stderr for PTY.
         *
         * @return -1 (stderr is merged with stdout through the PTY)
         */
        int stderr_fd() const override { return -1; }

        /**
         * @brief Get the SSH channel.
         *
         * @return Associated channel, or nullptr if not started
         */
        SshChannel *channel() const override { return _channel; }

        /**
         * @brief Check if the shell is still running.
         *
         * @return true if shell process is alive
         */
        bool is_running() const override;

    private:
        SshChannel *_channel;
        std::unique_ptr<Pty> _pty;

        // Configuration (stored until on_start is called)
        std::string _shell;
        std::vector<std::string> _args;
        std::vector<std::pair<std::string, std::string>> _env;
        std::string _working_dir;
};

} // namespace sihd::ssh

#endif
