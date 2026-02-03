#ifndef __SIHD_SSH_PTY_HPP__
#define __SIHD_SSH_PTY_HPP__

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sihd::ssh
{

/**
 * @brief Terminal size structure, portable across platforms.
 *
 * On POSIX systems, this maps directly to struct winsize.
 * On Windows, this is used to configure ConPTY dimensions.
 */
struct PtySize
{
        uint16_t cols;   ///< Number of columns (characters)
        uint16_t rows;   ///< Number of rows (characters)
        uint16_t xpixel; ///< Horizontal size in pixels (optional, 0 if unknown)
        uint16_t ypixel; ///< Vertical size in pixels (optional, 0 if unknown)
};

/**
 * @brief Abstract PTY (pseudo-terminal) interface.
 *
 * This interface abstracts platform-specific PTY implementations:
 * - **POSIX (Linux/macOS/BSD)**: Uses forkpty() or openpty()+fork()
 * - **Windows 10+**: Uses ConPTY (CreatePseudoConsole API)
 *
 * ## Usage Pattern
 * ```cpp
 * auto pty = Pty::create();
 * if (!pty) {
 *     // PTY not supported on this platform
 * }
 *
 * pty->set_shell("/bin/bash");
 * pty->set_size({80, 24, 0, 0});
 *
 * if (!pty->spawn()) {
 *     // Failed to spawn shell
 * }
 *
 * // Read/write loop
 * while (pty->is_running()) {
 *     // Write user input to PTY
 *     pty->write(input_data, input_len);
 *
 *     // Read PTY output (non-blocking)
 *     char buf[4096];
 *     ssize_t n = pty->read(buf, sizeof(buf));
 *     if (n > 0) {
 *         // Send to SSH channel
 *     }
 * }
 *
 * int exit_code = pty->wait();
 * ```
 *
 * ## Platform Support
 * - **Linux**: Full support via forkpty()
 * - **macOS**: Full support via forkpty()
 * - **Windows 10 1809+**: Support via ConPTY
 * - **Windows < 10 1809**: Not supported (create() returns nullptr)
 *
 * @see PosixPty, ConPty
 */
class Pty
{
    public:
        virtual ~Pty() = default;

        /**
         * @brief Factory method to create a platform-appropriate PTY.
         *
         * @return Unique pointer to PTY instance, or nullptr if PTY is not
         *         supported on this platform.
         *
         * On Windows, this will check if ConPTY is available (Windows 10 1809+)
         * and return nullptr if not supported.
         */
        static std::unique_ptr<Pty> create();

        /**
         * @brief Check if PTY is supported on the current platform.
         *
         * @return true if PTY can be created
         * @return false if PTY is not supported (e.g., old Windows versions)
         */
        static bool is_supported();

        // ===== Configuration (call before spawn()) =====

        /**
         * @brief Set the shell executable to run in the PTY.
         *
         * @param shell Path to shell executable (e.g., "/bin/bash", "cmd.exe")
         *
         * Defaults:
         * - POSIX: "/bin/sh"
         * - Windows: "cmd.exe"
         */
        virtual void set_shell(std::string_view shell) = 0;

        /**
         * @brief Set command-line arguments for the shell.
         *
         * @param args Arguments to pass to the shell
         *
         * Example:
         * ```cpp
         * pty->set_shell("/bin/bash");
         * pty->set_args({"-l"});  // Login shell
         * ```
         */
        virtual void set_args(std::vector<std::string> args) = 0;

        /**
         * @brief Set an environment variable for the spawned process.
         *
         * @param name Environment variable name
         * @param value Environment variable value
         *
         * Common variables to set:
         * - TERM: Terminal type (e.g., "xterm-256color")
         * - LANG: Locale (e.g., "en_US.UTF-8")
         */
        virtual void set_env(std::string_view name, std::string_view value) = 0;

        /**
         * @brief Set the initial PTY size.
         *
         * @param size Terminal dimensions
         *
         * Must be called before spawn(). Use resize() to change size after spawning.
         */
        virtual void set_size(const PtySize & size) = 0;

        /**
         * @brief Set the working directory for the spawned process.
         *
         * @param path Directory path
         */
        virtual void set_working_directory(std::string_view path) = 0;

        // ===== Lifecycle =====

        /**
         * @brief Spawn the shell process in the PTY.
         *
         * @return true if the process was successfully spawned
         * @return false on error (check system error for details)
         *
         * After successful spawn:
         * - read_fd() returns a valid file descriptor for reading PTY output
         * - write() can be used to send input to the PTY
         * - is_running() returns true
         */
        virtual bool spawn() = 0;

        /**
         * @brief Check if the spawned process is still running.
         *
         * @return true if process is running
         * @return false if process has exited or was never spawned
         *
         * This is non-blocking and can be called repeatedly.
         */
        virtual bool is_running() const = 0;

        /**
         * @brief Wait for the process to exit and return its exit code.
         *
         * @return Exit code of the process, or -1 if not spawned
         *
         * This is blocking. Call is_running() first if you need non-blocking behavior.
         */
        virtual int wait() = 0;

        /**
         * @brief Terminate the process.
         *
         * Attempts graceful termination first:
         * - POSIX: SIGHUP -> SIGTERM -> SIGKILL
         * - Windows: TerminateProcess
         */
        virtual void terminate() = 0;

        // ===== I/O Operations =====

        /**
         * @brief Get the file descriptor for reading PTY output.
         *
         * @return File descriptor, or -1 if not spawned
         *
         * The returned FD is set to non-blocking mode.
         * Use this with poll()/select() to wait for data.
         *
         * On Windows, this returns a special handle that can be used with
         * ReadFile() but NOT with select(). Use WaitForSingleObject() instead.
         */
        virtual int read_fd() const = 0;

        /**
         * @brief Read data from the PTY output.
         *
         * @param buffer Buffer to read into
         * @param size Maximum number of bytes to read
         * @return Number of bytes read, 0 if no data available, -1 on error
         *
         * This is non-blocking. Returns 0 immediately if no data is available.
         */
        virtual ssize_t read(void *buffer, size_t size) = 0;

        /**
         * @brief Write data to the PTY input.
         *
         * @param data Data to write
         * @param size Number of bytes to write
         * @return Number of bytes written, or -1 on error
         *
         * The data is sent to the shell's stdin.
         */
        virtual ssize_t write(const void *data, size_t size) = 0;

        /**
         * @brief Resize the PTY.
         *
         * @param size New terminal dimensions
         * @return true if resize was successful
         * @return false on error
         *
         * On POSIX, this sends SIGWINCH to the child process.
         * On Windows, this calls ResizePseudoConsole().
         */
        virtual bool resize(const PtySize & size) = 0;

        /**
         * @brief Send EOF to the PTY input.
         *
         * On POSIX, this closes the write side of the PTY.
         * On Windows, this sends Ctrl+Z or closes the input pipe.
         */
        virtual void send_eof() = 0;
};

} // namespace sihd::ssh

#endif
