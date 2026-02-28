#ifndef __SIHD_SSH_ISSHSUBSYSTEMHANDLER_HPP__
#define __SIHD_SSH_ISSHSUBSYSTEMHANDLER_HPP__

#include <cstddef>
#include <sys/ioctl.h>

namespace sihd::ssh
{

class SshChannel;

/**
 * Interface for SSH subsystem handlers (shell, exec, sftp, custom).
 *
 * Lifecycle: on_start → on_data → on_resize → on_eof → on_close
 */
class ISshSubsystemHandler
{
    public:
        virtual ~ISshSubsystemHandler() = default;

        // Returns true to accept the request
        virtual bool on_start(SshChannel *channel, bool has_pty, const struct winsize & winsize) = 0;
        // Returns bytes consumed, or -1 on error
        virtual int on_data(const void *data, size_t len) = 0;
        virtual void on_resize(const struct winsize & winsize) = 0;
        virtual void on_eof() = 0;
        // Returns exit code (0-255)
        virtual int on_close() = 0;

        // FDs to poll for process output (-1 if not applicable)
        virtual int stdout_fd() const = 0;
        virtual int stderr_fd() const = 0;

        virtual SshChannel *channel() const = 0;
        virtual bool is_running() const = 0;

        // When true, the server leaves channel data in libssh's buffers
        // for the handler to read directly (e.g. SFTP)
        virtual bool manages_channel_io() const { return false; }
};

} // namespace sihd::ssh

#endif
