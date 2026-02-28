#ifndef __SIHD_SSH_BASICSSHSERVERHANDLER_HPP__
#define __SIHD_SSH_BASICSSHSERVERHANDLER_HPP__

#include <sihd/ssh/ISshServerHandler.hpp>
#include <sihd/ssh/ISshSubsystemHandler.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sihd::ssh
{

class SshKey;

/**
 * Ready-to-use SSH server handler with user/password and pubkey auth,
 * multi-channel support, automatic handler lifecycle and FD polling.
 */
class BasicSshServerHandler: public ISshServerHandler
{
    public:
        // Handler factory callbacks - return nullptr to reject the request
        using ShellHandlerCallback = std::function<
            ISshSubsystemHandler
                *(SshSession *session, SshChannel *channel, bool has_pty, const struct winsize & winsize)>;

        using ExecHandlerCallback = std::function<ISshSubsystemHandler *(SshSession *session,
                                                                         SshChannel *channel,
                                                                         std::string_view command,
                                                                         bool has_pty,
                                                                         const struct winsize & winsize)>;

        using SubsystemHandlerCallback
            = std::function<ISshSubsystemHandler *(SshSession *session,
                                                   SshChannel *channel,
                                                   std::string_view subsystem,
                                                   bool has_pty,
                                                   const struct winsize & winsize)>;

        // Custom auth callbacks (called in addition to allowed user/key lists)
        using AuthPasswordCallback
            = std::function<bool(SshSession *session, std::string_view user, std::string_view password)>;
        using AuthPubkeyCallback
            = std::function<bool(SshSession *session, std::string_view user, const SshKey & key)>;

        BasicSshServerHandler();
        ~BasicSshServerHandler() override;

        // ===== Authentication =====

        void add_allowed_user(std::string_view user, std::string_view password);
        void remove_allowed_user(std::string_view user);
        void clear_allowed_users();
        void add_allowed_pubkey(std::string_view user, std::string_view pubkey_base64);
        void set_auth_password_callback(AuthPasswordCallback callback);
        void set_auth_pubkey_callback(AuthPubkeyCallback callback);

        // ===== Handler Callbacks =====

        void set_shell_handler_callback(ShellHandlerCallback callback);
        void set_exec_handler_callback(ExecHandlerCallback callback);
        void set_subsystem_handler_callback(SubsystemHandlerCallback callback);

        // ===== Defaults =====

        void set_default_exec_fork_mode(bool enable);
        void set_default_shell(std::string_view shell);

        // ===== Session Info =====

        uint64_t session_id(SshSession *session) const;
        size_t session_count() const;
        size_t channel_count(SshSession *session) const;

        // ===== Event Counters (for testing) =====

        struct EventCounters
        {
                size_t auth_password_success = 0;
                size_t auth_password_fail = 0;
                size_t auth_pubkey_success = 0;
                size_t auth_pubkey_fail = 0;
                size_t sessions_opened = 0;
                size_t sessions_closed = 0;
                size_t channels_opened = 0;
                size_t shell_requests = 0;
                size_t exec_requests = 0;
                size_t subsystem_requests = 0;
                size_t data_received = 0;
                size_t bytes_received = 0;
        };

        const EventCounters & counters() const { return _counters; }
        void reset_counters() { _counters = {}; }

        // ===== ISshServerHandler =====

        bool on_auth_password(SshServer *server,
                              SshSession *session,
                              std::string_view user,
                              std::string_view password) override;

        bool on_auth_pubkey(SshServer *server,
                            SshSession *session,
                            std::string_view user,
                            const SshKey & key) override;

        void on_session_opened(SshServer *server, SshSession *session) override;
        void on_session_closed(SshServer *server, SshSession *session) override;

        bool on_channel_open(SshServer *server, SshSession *session, SshChannel *channel) override;

        bool on_channel_request_pty(SshServer *server,
                                    SshSession *session,
                                    SshChannel *channel,
                                    std::string_view term,
                                    const struct winsize & size) override;

        bool on_channel_request_shell(SshServer *server, SshSession *session, SshChannel *channel) override;

        bool on_channel_request_exec(SshServer *server,
                                     SshSession *session,
                                     SshChannel *channel,
                                     std::string_view command) override;

        bool on_channel_request_subsystem(SshServer *server,
                                          SshSession *session,
                                          SshChannel *channel,
                                          std::string_view subsystem) override;

        void on_channel_data(SshServer *server,
                             SshSession *session,
                             SshChannel *channel,
                             const void *data,
                             size_t len,
                             bool is_stderr) override;

        void on_channel_pty_resize(SshServer *server,
                                   SshSession *session,
                                   SshChannel *channel,
                                   const struct winsize & size) override;

        void on_poll(SshServer *server) override;

        bool channel_bypasses_data_callback(SshChannel *channel) const override;

    private:
        struct ChannelState
        {
                std::unique_ptr<ISshSubsystemHandler> handler;
                bool has_pty;
                struct winsize winsize;
        };

        struct SessionState
        {
                uint64_t id;
                std::unordered_map<SshChannel *, ChannelState> channels;
        };

        ChannelState *get_channel_state(SshSession *session, SshChannel *channel);
        void poll_handler(SshChannel *channel, ISshSubsystemHandler *handler);

        std::unordered_map<std::string, std::string> _allowed_users;
        std::unordered_map<std::string, std::vector<std::string>> _allowed_pubkeys;
        AuthPasswordCallback _auth_password_callback;
        AuthPubkeyCallback _auth_pubkey_callback;

        ShellHandlerCallback _shell_callback;
        ExecHandlerCallback _exec_callback;
        SubsystemHandlerCallback _subsystem_callback;

        std::string _default_shell;
        bool _default_exec_fork_mode;

        std::unordered_map<SshSession *, SessionState> _sessions;
        uint64_t _next_session_id;

        EventCounters _counters;
};

} // namespace sihd::ssh

#endif
