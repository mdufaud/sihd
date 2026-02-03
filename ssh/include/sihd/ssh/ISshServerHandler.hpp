#ifndef __SIHD_SSH_ISSHSERVERHANDLER_HPP__
#define __SIHD_SSH_ISSHSERVERHANDLER_HPP__

#include <string_view>
#include <sys/ioctl.h>

namespace sihd::ssh
{

class SshSession;
class SshChannel;
class SshKey;
class SshServer;

class ISshServerHandler
{
    public:
        virtual ~ISshServerHandler() = default;

        // Authentication callbacks - return true to allow, false to deny
        virtual bool on_auth_password(SshServer *server,
                                      SshSession *session,
                                      std::string_view user,
                                      std::string_view password)
            = 0;
        virtual bool
            on_auth_pubkey(SshServer *server, SshSession *session, std::string_view user, const SshKey & key)
            = 0;

        // Session lifecycle callbacks
        virtual void on_session_opened(SshServer *server, SshSession *session) = 0;
        virtual void on_session_closed(SshServer *server, SshSession *session) = 0;

        // Channel request callbacks - return true to accept, false to deny
        virtual bool on_channel_open(SshServer *server, SshSession *session, SshChannel *channel) = 0;
        virtual bool on_channel_request_pty(SshServer *server,
                                            SshSession *session,
                                            SshChannel *channel,
                                            std::string_view term,
                                            const struct winsize & size)
            = 0;
        virtual bool on_channel_request_shell(SshServer *server, SshSession *session, SshChannel *channel)
            = 0;
        virtual bool on_channel_request_exec(SshServer *server,
                                             SshSession *session,
                                             SshChannel *channel,
                                             std::string_view command)
            = 0;
        virtual bool on_channel_request_subsystem(SshServer *server,
                                                  SshSession *session,
                                                  SshChannel *channel,
                                                  std::string_view subsystem)
            = 0;

        // Channel data callback
        virtual void on_channel_data(SshServer *server,
                                     SshSession *session,
                                     SshChannel *channel,
                                     const void *data,
                                     size_t len,
                                     bool is_stderr)
            = 0;

        // PTY resize callback
        virtual void on_channel_pty_resize(SshServer *server,
                                           SshSession *session,
                                           SshChannel *channel,
                                           const struct winsize & size)
            = 0;

        /**
         * Called on each iteration of the server's event loop.
         * Use this to poll child process FDs and forward data to SSH channels.
         *
         * This is where you should call poll() on child stdout/stderr FDs
         * and write any data to the SSH channel.
         *
         * @param server The SSH server
         */
        virtual void on_poll([[maybe_unused]] SshServer *server) {}

        /**
         * Check if a channel manages its own I/O and should not have data intercepted.
         *
         * When true, the SSH server will NOT consume channel data via callbacks.
         * Instead, data will be left in libssh's internal buffers for the handler
         * to read directly (e.g., SFTP using sftp_get_client_message).
         *
         * @param channel The channel to check
         * @return true if data should NOT be consumed by callback
         */
        virtual bool channel_bypasses_data_callback([[maybe_unused]] SshChannel *channel) const
        {
            return false;
        }
};

} // namespace sihd::ssh

#endif
