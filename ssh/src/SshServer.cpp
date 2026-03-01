#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>

#include <unordered_map>
#include <vector>

#include <libssh/callbacks.h>
#include <libssh/server.h>

#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/NamedFactory.hpp>

#include <sihd/ssh/ISshServerHandler.hpp>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshKey.hpp>
#include <sihd/ssh/SshServer.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/utils.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

namespace
{

// Per-channel data
struct ChannelData
{
        SshChannel *wrapper;
        bool has_pty;
        struct winsize winsize;
        struct ssh_channel_callbacks_struct callbacks;
};

// Session userdata for callbacks
struct SessionData
{
        SshServer *server;
        SshSession *session;
        std::vector<std::unique_ptr<ChannelData>> channels;
        int auth_attempts;
        bool authenticated;
        // For requests that come before channel is fully created (pty, etc.)
        bool pending_has_pty;
        struct winsize pending_winsize;

        // Callback structs must persist for the lifetime of the session
        struct ssh_server_callbacks_struct server_callbacks;

        ChannelData *find_channel(ssh_channel raw_channel)
        {
            for (auto & ch : channels)
            {
                if (ch && ch->wrapper && ch->wrapper->channel() == raw_channel)
                    return ch.get();
            }
            return nullptr;
        }

        ChannelData *find_channel(SshChannel *wrapper)
        {
            for (auto & ch : channels)
            {
                if (ch && ch->wrapper == wrapper)
                    return ch.get();
            }
            return nullptr;
        }
};

// Forward declarations for channel callbacks
int callback_channel_pty_request(ssh_session, ssh_channel, const char *, int, int, int, int, void *);
int callback_channel_pty_resize(ssh_session, ssh_channel, int, int, int, int, void *);
int callback_channel_shell_request(ssh_session, ssh_channel, void *);
int callback_channel_exec_request(ssh_session, ssh_channel, const char *, void *);
int callback_channel_subsystem_request(ssh_session, ssh_channel, const char *, void *);
int callback_channel_data(ssh_session, ssh_channel, void *, uint32_t, int, void *);

// Global callbacks for libssh
int callback_auth_password(ssh_session session, const char *user, const char *password, void *userdata)
{
    (void)session;
    auto *data = static_cast<SessionData *>(userdata);
    if (data->server->server_handler() == nullptr)
    {
        SIHD_LOG(warning, "SshServer: no handler set, denying auth");
        return SSH_AUTH_DENIED;
    }

    data->auth_attempts++;
    if (data->auth_attempts >= 3)
        return SSH_AUTH_DENIED;

    bool allowed = data->server->server_handler()->on_auth_password(data->server,
                                                                    data->session,
                                                                    user ? user : "",
                                                                    password ? password : "");
    if (allowed)
    {
        data->authenticated = true;
        return SSH_AUTH_SUCCESS;
    }
    return SSH_AUTH_DENIED;
}

int callback_auth_pubkey(ssh_session session,
                         const char *user,
                         struct ssh_key_struct *pubkey,
                         char signature_state,
                         void *userdata)
{
    auto *data = static_cast<SessionData *>(userdata);
    (void)session;

    if (signature_state == SSH_PUBLICKEY_STATE_NONE)
        return SSH_AUTH_SUCCESS;

    if (signature_state != SSH_PUBLICKEY_STATE_VALID)
        return SSH_AUTH_DENIED;

    if (data->server->server_handler() == nullptr)
        return SSH_AUTH_DENIED;

    // Duplicate the key since libssh owns the original and will free it
    ssh_key key_dup = ssh_key_dup(pubkey);
    if (key_dup == nullptr)
        return SSH_AUTH_DENIED;

    SshKey key(key_dup);
    bool allowed
        = data->server->server_handler()->on_auth_pubkey(data->server, data->session, user ? user : "", key);
    if (allowed)
    {
        data->authenticated = true;
        return SSH_AUTH_SUCCESS;
    }
    return SSH_AUTH_DENIED;
}

ssh_channel callback_channel_open(ssh_session session, void *userdata)
{
    auto *data = static_cast<SessionData *>(userdata);
    (void)session;

    ssh_channel raw_channel = ssh_channel_new(session);
    if (raw_channel == nullptr)
        return nullptr;

    // Create channel data
    auto ch_data = std::make_unique<ChannelData>();
    ch_data->wrapper = new SshChannel(raw_channel);
    ch_data->wrapper->set_userdata(data);
    ch_data->has_pty = data->pending_has_pty; // Use pending values if set before channel open
    ch_data->winsize = data->pending_winsize;

    // Setup channel callbacks (stored in ChannelData to persist)
    ssh_callbacks_init(&ch_data->callbacks);
    ch_data->callbacks.userdata = data;
    ch_data->callbacks.channel_pty_request_function = callback_channel_pty_request;
    ch_data->callbacks.channel_pty_window_change_function = callback_channel_pty_resize;
    ch_data->callbacks.channel_shell_request_function = callback_channel_shell_request;
    ch_data->callbacks.channel_exec_request_function = callback_channel_exec_request;
    ch_data->callbacks.channel_subsystem_request_function = callback_channel_subsystem_request;
    ch_data->callbacks.channel_data_function = callback_channel_data;

    ssh_set_channel_callbacks(raw_channel, &ch_data->callbacks);

    if (data->server->server_handler())
    {
        bool allowed
            = data->server->server_handler()->on_channel_open(data->server, data->session, ch_data->wrapper);
        if (!allowed)
        {
            delete ch_data->wrapper;
            ssh_channel_free(raw_channel);
            return nullptr;
        }
    }

    data->channels.push_back(std::move(ch_data));

    // Reset pending values
    data->pending_has_pty = false;
    data->pending_winsize = {};

    SIHD_LOG(debug, "SshServer: channel opened (total: {})", data->channels.size());
    return raw_channel;
}

int callback_channel_pty_request(ssh_session session,
                                 ssh_channel channel,
                                 const char *term,
                                 int cols,
                                 int rows,
                                 int py,
                                 int px,
                                 void *userdata)
{
    auto *data = static_cast<SessionData *>(userdata);
    (void)session;

    struct winsize ws = {.ws_row = static_cast<unsigned short>(rows),
                         .ws_col = static_cast<unsigned short>(cols),
                         .ws_xpixel = static_cast<unsigned short>(px),
                         .ws_ypixel = static_cast<unsigned short>(py)};

    // Find the channel
    ChannelData *ch_data = data->find_channel(channel);
    if (ch_data)
    {
        ch_data->has_pty = true;
        ch_data->winsize = ws;
    }
    else
    {
        // Store as pending for next channel open
        data->pending_has_pty = true;
        data->pending_winsize = ws;
    }

    if (data->server->server_handler() && ch_data)
    {
        bool allowed = data->server->server_handler()->on_channel_request_pty(data->server,
                                                                              data->session,
                                                                              ch_data->wrapper,
                                                                              term ? term : "",
                                                                              ws);
        return allowed ? SSH_OK : SSH_ERROR;
    }
    return SSH_OK;
}

int callback_channel_pty_resize(ssh_session session,
                                ssh_channel channel,
                                int cols,
                                int rows,
                                int py,
                                int px,
                                void *userdata)
{
    auto *data = static_cast<SessionData *>(userdata);
    (void)session;

    ChannelData *ch_data = data->find_channel(channel);
    if (!ch_data)
        return SSH_ERROR;

    ch_data->winsize.ws_row = rows;
    ch_data->winsize.ws_col = cols;
    ch_data->winsize.ws_xpixel = px;
    ch_data->winsize.ws_ypixel = py;

    if (data->server->server_handler())
    {
        data->server->server_handler()->on_channel_pty_resize(data->server,
                                                              data->session,
                                                              ch_data->wrapper,
                                                              ch_data->winsize);
    }
    return SSH_OK;
}

int callback_channel_shell_request(ssh_session session, ssh_channel channel, void *userdata)
{
    auto *data = static_cast<SessionData *>(userdata);
    (void)session;

    ChannelData *ch_data = data->find_channel(channel);
    if (!ch_data)
        return SSH_ERROR;

    if (data->server->server_handler())
    {
        bool allowed = data->server->server_handler()->on_channel_request_shell(data->server,
                                                                                data->session,
                                                                                ch_data->wrapper);
        return allowed ? SSH_OK : SSH_ERROR;
    }
    return SSH_ERROR;
}

int callback_channel_exec_request(ssh_session session,
                                  ssh_channel channel,
                                  const char *command,
                                  void *userdata)
{
    auto *data = static_cast<SessionData *>(userdata);
    (void)session;

    ChannelData *ch_data = data->find_channel(channel);
    if (!ch_data)
        return SSH_ERROR;

    if (data->server->server_handler())
    {
        bool allowed = data->server->server_handler()->on_channel_request_exec(data->server,
                                                                               data->session,
                                                                               ch_data->wrapper,
                                                                               command ? command : "");
        return allowed ? SSH_OK : SSH_ERROR;
    }
    return SSH_ERROR;
}

int callback_channel_subsystem_request([[maybe_unused]] ssh_session session,
                                       ssh_channel channel,
                                       const char *subsystem,
                                       void *userdata)
{
    auto *data = static_cast<SessionData *>(userdata);

    ChannelData *ch_data = data->find_channel(channel);
    if (!ch_data)
        return SSH_ERROR;

    if (data->server->server_handler())
    {
        bool allowed
            = data->server->server_handler()->on_channel_request_subsystem(data->server,
                                                                           data->session,
                                                                           ch_data->wrapper,
                                                                           subsystem ? subsystem : "");
        return allowed ? SSH_OK : SSH_ERROR;
    }
    return SSH_ERROR;
}

int callback_channel_data([[maybe_unused]] ssh_session session,
                          ssh_channel channel,
                          void *data_buf,
                          uint32_t len,
                          int is_stderr,
                          void *userdata)
{
    auto *data = static_cast<SessionData *>(userdata);

    ChannelData *ch_data = data->find_channel(channel);
    if (!ch_data)
        return len;

    // If the channel handler manages its own I/O (e.g., SFTP), don't consume the data.
    // Return 0 to leave it in libssh's internal buffers for ssh_channel_read().
    if (data->server->server_handler()
        && data->server->server_handler()->channel_bypasses_data_callback(ch_data->wrapper))
        return 0;

    if (data->server->server_handler())
    {
        data->server->server_handler()
            ->on_channel_data(data->server, data->session, ch_data->wrapper, data_buf, len, is_stderr != 0);
    }
    return len;
}

} // namespace

struct SshServer::Impl
{
        ssh_bind bind;
        ssh_event event;
        std::vector<std::unique_ptr<SshSession>> sessions;
        std::unordered_map<ssh_session, SessionData *> session_data;

        int port;
        int actual_port;
        std::string bind_addr;
        std::string rsa_key;
        std::string dsa_key;
        std::string ecdsa_key;
        std::string authorized_keys;
        std::string banner;
        int verbosity;

        Impl():
            bind(nullptr),
            event(nullptr),
            port(22),
            actual_port(0),
            bind_addr("0.0.0.0"),
            verbosity(SSH_LOG_NOLOG)
        {
        }

        ~Impl() { cleanup(); }

        void cleanup()
        {
            // Clean up sessions - remove from event first
            for (auto & [ssh_session_ptr, data] : session_data)
            {
                if (event)
                {
                    ssh_event_remove_session(event, ssh_session_ptr);
                }
                if (data)
                {
                    for (auto & ch_data : data->channels)
                    {
                        if (ch_data && ch_data->wrapper)
                        {
                            ch_data->wrapper->detach();
                            delete ch_data->wrapper;
                        }
                    }
                    data->channels.clear();
                    delete data;
                }
            }
            session_data.clear();
            sessions.clear();

            if (event)
            {
                ssh_event_free(event);
                event = nullptr;
            }

            if (bind)
            {
                ssh_bind_free(bind);
                bind = nullptr;
            }
        }

        bool setup_bind(SshServer *server)
        {
            (void)server;
            bind = ssh_bind_new();
            if (bind == nullptr)
            {
                SIHD_LOG(error, "SshServer: failed to create ssh_bind");
                return false;
            }

            // Disable automatic loading of system configuration files
            // This prevents libssh from trying to load system keys like /etc/ssh/ssh_host_ecdsa_key
            int process_config = 0;
            ssh_bind_options_set(bind, SSH_BIND_OPTIONS_PROCESS_CONFIG, &process_config);

            // Set options
            ssh_bind_options_set(bind, SSH_BIND_OPTIONS_BINDADDR, bind_addr.c_str());
            ssh_bind_options_set(bind, SSH_BIND_OPTIONS_BINDPORT, &port);

            // Use SSH_BIND_OPTIONS_HOSTKEY for all key types (modern API)
            if (!rsa_key.empty())
                ssh_bind_options_set(bind, SSH_BIND_OPTIONS_HOSTKEY, rsa_key.c_str());
            if (!dsa_key.empty())
                ssh_bind_options_set(bind, SSH_BIND_OPTIONS_HOSTKEY, dsa_key.c_str());
            if (!ecdsa_key.empty())
                ssh_bind_options_set(bind, SSH_BIND_OPTIONS_HOSTKEY, ecdsa_key.c_str());

            if (verbosity > 0)
            {
                ssh_bind_options_set(bind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &verbosity);
            }

            if (ssh_bind_listen(bind) < 0)
            {
                SIHD_LOG(error, "SshServer: failed to listen: {}", ssh_get_error(bind));
                return false;
            }

            // Get actual port if port was 0 (dynamic allocation)
            if (port == 0)
            {
                socket_t fd = ssh_bind_get_fd(bind);
                if (fd != SSH_INVALID_SOCKET)
                {
                    struct sockaddr_in addr;
                    socklen_t addr_len = sizeof(addr);
                    if (getsockname(fd, (struct sockaddr *)&addr, &addr_len) == 0)
                    {
                        actual_port = ntohs(addr.sin_port);
                    }
                    else
                    {
                        SIHD_LOG(warning, "SshServer: failed to get actual port");
                        actual_port = port;
                    }
                }
            }
            else
            {
                actual_port = port;
            }

            // Set bind to non-blocking so accept doesn't block
            ssh_bind_set_blocking(bind, 0);

            SIHD_LOG(info, "SshServer: listening on {}:{}", bind_addr, actual_port);
            return true;
        }

        bool accept_session(SshServer *server)
        {
            ssh_session session = ssh_new();
            if (session == nullptr)
            {
                SIHD_LOG(error, "SshServer: failed to create session");
                return false;
            }

            // Accept with non-blocking bind
            int rc = ssh_bind_accept(bind, session);
            if (rc == SSH_AGAIN)
            {
                // No connection waiting, normal for non-blocking
                ssh_free(session);
                return true;
            }

            if (rc == SSH_ERROR)
            {
                SIHD_LOG(error, "SshServer: accept failed: {}", ssh_get_error(bind));
                ssh_free(session);
                return false;
            }

            SIHD_LOG(info, "SshServer: accepted new connection");

            // Create session wrapper (session is still in blocking mode)
            auto session_wrapper = std::make_unique<SshSession>(session);

            // Setup session data for callbacks
            auto *data = new SessionData {.server = server,
                                          .session = session_wrapper.get(),
                                          .channels = {},
                                          .auth_attempts = 0,
                                          .authenticated = false,
                                          .pending_has_pty = false,
                                          .pending_winsize = {0, 0, 0, 0},
                                          .server_callbacks = {}};

            session_wrapper->set_userdata(data);
            session_data[session] = data;

            // Setup server callbacks (stored in SessionData to persist)
            ssh_callbacks_init(&data->server_callbacks);
            data->server_callbacks.userdata = data;
            data->server_callbacks.auth_password_function = callback_auth_password;
            data->server_callbacks.auth_pubkey_function = callback_auth_pubkey;
            data->server_callbacks.channel_open_request_session_function = callback_channel_open;

            // Set auth methods
            if (!authorized_keys.empty())
                ssh_set_auth_methods(session, SSH_AUTH_METHOD_PASSWORD | SSH_AUTH_METHOD_PUBLICKEY);
            else
                ssh_set_auth_methods(session, SSH_AUTH_METHOD_PASSWORD);

            ssh_set_server_callbacks(session, &data->server_callbacks);

            // Channel callbacks are now set per-channel in callback_channel_open

            // Perform key exchange (session is in blocking mode)
            if (ssh_handle_key_exchange(session) != SSH_OK)
            {
                SIHD_LOG(error, "SshServer: key exchange failed: {}", ssh_get_error(session));
                delete data;
                session_data.erase(session);
                return false;
            }

            SIHD_LOG(debug, "SshServer: key exchange completed");

            // NOW set session to non-blocking for event loop
            ssh_set_blocking(session, 0);

            // Add to event loop to handle subsequent messages
            ssh_event_add_session(event, session);

            SIHD_LOG(debug, "SshServer: session added to event loop");

            // Notify handler
            if (server->server_handler())
            {
                server->server_handler()->on_session_opened(server, session_wrapper.get());
            }

            sessions.push_back(std::move(session_wrapper));
            return true;
        }

        void cleanup_session(ssh_session session)
        {
            auto it = session_data.find(session);
            if (it != session_data.end())
            {
                if (it->second)
                {
                    for (auto & ch_data : it->second->channels)
                    {
                        if (ch_data && ch_data->wrapper)
                        {
                            // Detach first - libssh already freed the channel when session closed
                            ch_data->wrapper->detach();
                            delete ch_data->wrapper;
                        }
                    }
                    it->second->channels.clear();
                    delete it->second;
                }
                session_data.erase(it);
            }

            ssh_event_remove_session(event, session);

            // Remove from sessions vector
            sessions.erase(std::remove_if(sessions.begin(),
                                          sessions.end(),
                                          [session](const std::unique_ptr<SshSession> & s) {
                                              return s->session() == session;
                                          }),
                           sessions.end());
        }

        void cleanup_closed_sessions(SshServer *server)
        {
            std::vector<ssh_session> to_remove;

            for (auto & session_wrapper : sessions)
            {
                ssh_session session = static_cast<ssh_session>(session_wrapper->session());

                // Check if session is closed (not just disconnected)
                int status = ssh_get_status(session);
                if ((status & (SSH_CLOSED | SSH_CLOSED_ERROR)) != 0)
                {
                    SIHD_LOG(debug, "SshServer: session closed, status={}", status);
                    to_remove.push_back(session);

                    // Notify handler
                    if (server->server_handler())
                    {
                        server->server_handler()->on_session_closed(server, session_wrapper.get());
                    }
                }
            }

            for (auto session : to_remove)
            {
                this->cleanup_session(session);
            }
        }

        bool run_event_loop(SshServer *server)
        {
            event = ssh_event_new();
            if (event == nullptr)
            {
                SIHD_LOG(error, "SshServer: failed to create event");
                return false;
            }

            int bind_fd = ssh_bind_get_fd(bind);
            if (bind_fd < 0)
            {
                SIHD_LOG(error, "SshServer: failed to get bind fd");
                return false;
            }

            server->service_set_ready();

            while (!server->should_stop())
            {
                struct pollfd pfd = {.fd = bind_fd, .events = POLLIN, .revents = 0};
                constexpr int poll_timeout_ms = 0;
                int poll_rc = poll(&pfd, 1, poll_timeout_ms);

                if (poll_rc > 0 && (pfd.revents & POLLIN))
                {
                    if (!this->accept_session(server))
                    {
                        SIHD_LOG(error, "SshServer: failed to accept session");
                    }
                }
                else if (poll_rc < 0 && errno != EINTR)
                {
                    SIHD_LOG(error, "SshServer: poll error: {}", strerror(errno));
                    break;
                }

                // Process all sessions
                constexpr int event_poll_timeout_ms = 0;
                ssh_event_dopoll(event, event_poll_timeout_ms);

                // Allow handler to poll child FDs
                if (server->server_handler())
                {
                    server->server_handler()->on_poll(server);
                }

                this->cleanup_closed_sessions(server);
            }

            return true;
        }
};

SIHD_REGISTER_FACTORY(SshServer)

SshServer::SshServer(const std::string & name, sihd::util::Node *parent):
    sihd::util::Named(name, parent),
    sihd::util::Configurable(),
    _server_handler_ptr(nullptr),
    _stop(false)
{
    _impl_ptr = std::make_unique<Impl>();
    utils::init();
}

SshServer::~SshServer()
{
    if (this->is_running())
        this->stop();
    utils::finalize();
}

bool SshServer::set_port(int port)
{
    if (this->is_running())
    {
        SIHD_LOG(error, "SshServer: cannot change port while running");
        return false;
    }
    _impl_ptr->port = port;
    return true;
}

bool SshServer::set_bind_address(std::string_view addr)
{
    if (this->is_running())
    {
        SIHD_LOG(error, "SshServer: cannot change bind address while running");
        return false;
    }
    _impl_ptr->bind_addr = addr;
    return true;
}

bool SshServer::add_host_key(std::string_view key_path)
{
    // For now, just set as RSA key (user should use specific methods)
    return set_rsa_key(key_path);
}

bool SshServer::set_rsa_key(std::string_view key_path)
{
    if (this->is_running())
    {
        SIHD_LOG(error, "SshServer: cannot change keys while running");
        return false;
    }
    _impl_ptr->rsa_key = key_path;
    return true;
}

bool SshServer::set_dsa_key(std::string_view key_path)
{
    if (this->is_running())
    {
        SIHD_LOG(error, "SshServer: cannot change keys while running");
        return false;
    }
    _impl_ptr->dsa_key = key_path;
    return true;
}

bool SshServer::set_ecdsa_key(std::string_view key_path)
{
    if (this->is_running())
    {
        SIHD_LOG(error, "SshServer: cannot change keys while running");
        return false;
    }
    _impl_ptr->ecdsa_key = key_path;
    return true;
}

bool SshServer::set_authorized_keys_file(std::string_view path)
{
    if (this->is_running())
    {
        SIHD_LOG(error, "SshServer: cannot change authorized_keys while running");
        return false;
    }
    _impl_ptr->authorized_keys = path;
    return true;
}

bool SshServer::set_banner(std::string_view banner)
{
    if (this->is_running())
    {
        SIHD_LOG(error, "SshServer: cannot change banner while running");
        return false;
    }
    _impl_ptr->banner = banner;
    return true;
}

bool SshServer::set_verbosity(int level)
{
    _impl_ptr->verbosity = level;
    return true;
}

void SshServer::set_server_handler(ISshServerHandler *handler)
{
    _server_handler_ptr = handler;
}

int SshServer::get_port() const
{
    // Return actual_port if it was determined (after listening)
    // Otherwise return configured port
    return _impl_ptr->actual_port != 0 ? _impl_ptr->actual_port : _impl_ptr->port;
}

bool SshServer::on_start()
{
    _stop = false;

    sihd::util::Defer defer_finalize([this] { _impl_ptr->cleanup(); });

    return _impl_ptr->setup_bind(this) && _impl_ptr->run_event_loop(this);
}

bool SshServer::on_stop()
{
    _stop = true;
    return true;
}

} // namespace sihd::ssh
