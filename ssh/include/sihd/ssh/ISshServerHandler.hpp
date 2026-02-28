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

        // Authentication - return true to allow
        virtual bool on_auth_password(SshServer *server,
                                      SshSession *session,
                                      std::string_view user,
                                      std::string_view password)
            = 0;
        virtual bool
            on_auth_pubkey(SshServer *server, SshSession *session, std::string_view user, const SshKey & key)
            = 0;

        // Session lifecycle
        virtual void on_session_opened(SshServer *server, SshSession *session) = 0;
        virtual void on_session_closed(SshServer *server, SshSession *session) = 0;

        // Channel requests - return true to accept
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

        virtual void on_channel_data(SshServer *server,
                                     SshSession *session,
                                     SshChannel *channel,
                                     const void *data,
                                     size_t len,
                                     bool is_stderr)
            = 0;

        virtual void on_channel_pty_resize(SshServer *server,
                                           SshSession *session,
                                           SshChannel *channel,
                                           const struct winsize & size)
            = 0;

        // Called on each event loop iteration (poll child FDs, forward data, etc.)
        virtual void on_poll([[maybe_unused]] SshServer *server) {}

        // When true, channel data is left in libssh's buffers for the handler to read directly
        virtual bool channel_bypasses_data_callback([[maybe_unused]] SshChannel *channel) const
        {
            return false;
        }
};

} // namespace sihd::ssh

#endif
