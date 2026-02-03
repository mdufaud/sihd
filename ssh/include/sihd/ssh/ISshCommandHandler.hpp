#ifndef __SIHD_SSH_ISSHCOMMANDHANDLER_HPP__
#define __SIHD_SSH_ISSHCOMMANDHANDLER_HPP__

#include <string_view>
#include <sys/ioctl.h>

namespace sihd::ssh
{

class SshChannel;

/**
 * Interface for handling SSH command execution (shell, exec, subsystem).
 *
 * Implementations can either:
 * - Handle commands in the event loop (custom shell, echo server, etc.)
 * - Fork/exec external processes with PTY (traditional shell)
 *
 * The handler returns file descriptors for stdout/stderr that will be
 * monitored by the server's event loop and forwarded to the SSH channel.
 *
 * For in-process handlers, return -1 for stdout_fd/stderr_fd and use
 * the channel's write methods directly in on_stdin_data().
 */
class ISshCommandHandler
{
    public:
        virtual ~ISshCommandHandler() = default;

        /**
         * Called when a shell, exec, or subsystem is requested.
         *
         * @param channel The SSH channel
         * @param command The command to execute (empty for shell requests)
         * @param subsystem The subsystem name (empty for shell/exec requests)
         * @param has_pty True if PTY was requested
         * @param winsize PTY window size (if has_pty is true)
         * @param stdout_fd[out] Set to stdout FD to monitor, or -1 for none
         * @param stderr_fd[out] Set to stderr FD to monitor, or -1 for none
         * @return true on success, false to deny the request
         */
        virtual bool on_start(SshChannel *channel,
                              std::string_view command,
                              std::string_view subsystem,
                              bool has_pty,
                              const struct winsize & winsize,
                              int & stdout_fd,
                              int & stderr_fd)
            = 0;

        /**
         * Called when data is received on stdin (from SSH client).
         *
         * @param data Data buffer
         * @param len Data length
         * @return Number of bytes consumed, or -1 on error
         */
        virtual int on_stdin_data(const void *data, size_t len) = 0;

        /**
         * Called when PTY window size changes.
         * Only called if has_pty was true in on_start().
         */
        virtual void on_resize(const struct winsize & winsize) = 0;

        /**
         * Called when the channel is closing or the command should terminate.
         * Should clean up resources and close file descriptors.
         *
         * @return Exit status code (0-255)
         */
        virtual int on_stop() = 0;
};

} // namespace sihd::ssh

#endif
