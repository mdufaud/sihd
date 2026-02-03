#ifndef __SIHD_SSH_ISSHSUBSYSTEMHANDLER_HPP__
#define __SIHD_SSH_ISSHSUBSYSTEMHANDLER_HPP__

#include <cstddef>
#include <sys/ioctl.h>

namespace sihd::ssh
{

class SshChannel;

/**
 * Interface for SSH subsystem handlers (shell, exec, scp, sftp, custom).
 *
 * Each handler manages one type of channel request:
 * - Shell: interactive shell session
 * - Exec: single command execution
 * - Subsystem: named subsystem (sftp, scp, etc.)
 *
 * The handler receives data from the SSH client and can write responses
 * back via the SshChannel. For handlers that spawn external processes,
 * stdout_fd() and stderr_fd() return file descriptors to poll.
 *
 * Lifecycle:
 * 1. on_start() - Initialize handler, return true to accept
 * 2. on_data() - Called for each data packet from client
 * 3. on_resize() - Called when PTY window changes (if has_pty)
 * 4. on_eof() - Called when client sends EOF
 * 5. on_close() - Cleanup, return exit code
 */
class ISshSubsystemHandler
{
    public:
        virtual ~ISshSubsystemHandler() = default;

        /**
         * Start the handler.
         * @param channel The SSH channel to use for I/O
         * @param has_pty Whether a PTY was allocated
         * @param winsize PTY window size (valid if has_pty)
         * @return true to accept, false to reject
         */
        virtual bool on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize) = 0;

        /**
         * Handle data from the SSH client.
         * @param data Incoming data buffer
         * @param len Data length
         * @return Number of bytes consumed, or -1 on error
         */
        virtual int on_data(const void *data, size_t len) = 0;

        /**
         * Handle PTY resize.
         * @param winsize New window size
         */
        virtual void on_resize(const struct winsize & winsize) = 0;

        /**
         * Called when client sends EOF.
         */
        virtual void on_eof() = 0;

        /**
         * Cleanup and return exit code.
         * @return Exit code (0-255)
         */
        virtual int on_close() = 0;

        /**
         * Get stdout file descriptor for polling (-1 if not applicable).
         * For handlers that spawn processes, return the FD to poll for output.
         */
        virtual int stdout_fd() const = 0;

        /**
         * Get stderr file descriptor for polling (-1 if not applicable).
         */
        virtual int stderr_fd() const = 0;

        /**
         * Get the associated channel.
         */
        virtual SshChannel *channel() const = 0;

        /**
         * Check if handler is still active/running.
         */
        virtual bool is_running() const = 0;

        /**
         * Return true if this handler manages its own channel I/O.
         *
         * Handlers like SFTP use libssh's internal SFTP API which reads
         * directly from the SSH channel. For these handlers, the SSH server
         * should NOT intercept channel data via callbacks - it should leave
         * the data in libssh's internal buffers for the handler to read.
         *
         * @return true if the handler reads from the channel directly (SFTP)
         *         false if the handler expects data via on_data() (shell, exec)
         */
        virtual bool manages_channel_io() const { return false; }
};

} // namespace sihd::ssh

#endif
