#include <poll.h>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>

#include <sihd/ssh/BasicSshServerHandler.hpp>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshKey.hpp>
#include <sihd/ssh/SshServer.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/SshSubsystemExec.hpp>
#include <sihd/ssh/utils.hpp>

namespace sihd::ssh
{

SIHD_LOGGER;

BasicSshServerHandler::BasicSshServerHandler(): _default_exec_fork_mode(false), _next_session_id(1)
{
    utils::init();
}

BasicSshServerHandler::~BasicSshServerHandler()
{
    _sessions.clear();
    utils::finalize();
}

// ===== Authentication Configuration =====

void BasicSshServerHandler::add_allowed_user(std::string_view user, std::string_view password)
{
    _allowed_users[std::string(user)] = std::string(password);
}

void BasicSshServerHandler::remove_allowed_user(std::string_view user)
{
    _allowed_users.erase(std::string(user));
}

void BasicSshServerHandler::clear_allowed_users()
{
    _allowed_users.clear();
}

void BasicSshServerHandler::add_allowed_pubkey(std::string_view user, std::string_view pubkey_base64)
{
    _allowed_pubkeys[std::string(user)].emplace_back(pubkey_base64);
}

void BasicSshServerHandler::set_auth_password_callback(AuthPasswordCallback callback)
{
    _auth_password_callback = std::move(callback);
}

void BasicSshServerHandler::set_auth_pubkey_callback(AuthPubkeyCallback callback)
{
    _auth_pubkey_callback = std::move(callback);
}

// ===== Handler Callbacks =====

void BasicSshServerHandler::set_shell_handler_callback(ShellHandlerCallback callback)
{
    _shell_callback = std::move(callback);
}

void BasicSshServerHandler::set_exec_handler_callback(ExecHandlerCallback callback)
{
    _exec_callback = std::move(callback);
}

void BasicSshServerHandler::set_subsystem_handler_callback(SubsystemHandlerCallback callback)
{
    _subsystem_callback = std::move(callback);
}

// ===== Defaults Configuration =====

void BasicSshServerHandler::set_default_exec_fork_mode(bool enable)
{
    _default_exec_fork_mode = enable;
}

void BasicSshServerHandler::set_default_shell(std::string_view shell)
{
    _default_shell = shell;
}

// ===== Session Info =====

uint64_t BasicSshServerHandler::session_id(SshSession *session) const
{
    auto it = _sessions.find(session);
    if (it != _sessions.end())
    {
        return it->second.id;
    }
    return 0;
}

size_t BasicSshServerHandler::session_count() const
{
    return _sessions.size();
}

size_t BasicSshServerHandler::channel_count(SshSession *session) const
{
    auto it = _sessions.find(session);
    if (it != _sessions.end())
    {
        return it->second.channels.size();
    }
    return 0;
}

// ===== ISshServerHandler Implementation =====

bool BasicSshServerHandler::on_auth_password([[maybe_unused]] SshServer *server,
                                             SshSession *session,
                                             std::string_view user,
                                             std::string_view password)
{
    // Check allowed users list first
    auto it = _allowed_users.find(std::string(user));
    if (it != _allowed_users.end() && it->second == password)
    {
        SIHD_LOG(debug, "BasicSshServerHandler: password auth success for user '{}'", user);
        ++_counters.auth_password_success;
        return true;
    }

    // Try custom callback
    if (_auth_password_callback && _auth_password_callback(session, user, password))
    {
        SIHD_LOG(debug, "BasicSshServerHandler: password auth (callback) success for user '{}'", user);
        ++_counters.auth_password_success;
        return true;
    }

    SIHD_LOG(debug, "BasicSshServerHandler: password auth failed for user '{}'", user);
    ++_counters.auth_password_fail;
    return false;
}

bool BasicSshServerHandler::on_auth_pubkey([[maybe_unused]] SshServer *server,
                                           SshSession *session,
                                           std::string_view user,
                                           const SshKey & key)
{
    // Check allowed pubkeys list
    auto it = _allowed_pubkeys.find(std::string(user));
    if (it != _allowed_pubkeys.end())
    {
        std::string key_base64 = key.base64();
        for (const auto & allowed_key : it->second)
        {
            if (allowed_key == key_base64)
            {
                SIHD_LOG(debug, "BasicSshServerHandler: pubkey auth success for user '{}'", user);
                ++_counters.auth_pubkey_success;
                return true;
            }
        }
    }

    // Try custom callback
    if (_auth_pubkey_callback && _auth_pubkey_callback(session, user, key))
    {
        SIHD_LOG(debug, "BasicSshServerHandler: pubkey auth (callback) success for user '{}'", user);
        ++_counters.auth_pubkey_success;
        return true;
    }

    SIHD_LOG(debug, "BasicSshServerHandler: pubkey auth failed for user '{}'", user);
    ++_counters.auth_pubkey_fail;
    return false;
}

void BasicSshServerHandler::on_session_opened([[maybe_unused]] SshServer *server, SshSession *session)
{
    SessionState state;
    state.id = _next_session_id++;
    _sessions[session] = std::move(state);

    ++_counters.sessions_opened;
    SIHD_LOG(debug, "BasicSshServerHandler: session {} opened", _sessions[session].id);
}

void BasicSshServerHandler::on_session_closed([[maybe_unused]] SshServer *server, SshSession *session)
{
    auto it = _sessions.find(session);
    if (it != _sessions.end())
    {
        SIHD_LOG(debug,
                 "BasicSshServerHandler: session {} closed ({} channels)",
                 it->second.id,
                 it->second.channels.size());
        _sessions.erase(it);
    }
    ++_counters.sessions_closed;
}

bool BasicSshServerHandler::on_channel_open([[maybe_unused]] SshServer *server,
                                            SshSession *session,
                                            SshChannel *channel)
{
    auto it = _sessions.find(session);
    if (it == _sessions.end())
    {
        SIHD_LOG(error, "BasicSshServerHandler: channel open for unknown session");
        return false;
    }

    ChannelState ch_state;
    ch_state.has_pty = false;
    ch_state.winsize = {};
    it->second.channels[channel] = std::move(ch_state);

    ++_counters.channels_opened;
    SIHD_LOG(debug,
             "BasicSshServerHandler: channel opened for session {} (total: {})",
             it->second.id,
             it->second.channels.size());
    return true;
}

bool BasicSshServerHandler::on_channel_request_pty([[maybe_unused]] SshServer *server,
                                                   SshSession *session,
                                                   SshChannel *channel,
                                                   [[maybe_unused]] std::string_view term,
                                                   const struct winsize & size)
{
    ChannelState *state = get_channel_state(session, channel);
    if (!state)
    {
        return false;
    }

    state->has_pty = true;
    state->winsize = size;

    SIHD_LOG(debug, "BasicSshServerHandler: PTY requested ({}x{})", size.ws_col, size.ws_row);
    return true;
}

bool BasicSshServerHandler::on_channel_request_shell([[maybe_unused]] SshServer *server,
                                                     SshSession *session,
                                                     SshChannel *channel)
{
    ChannelState *state = get_channel_state(session, channel);
    if (!state)
    {
        return false;
    }

    ISshSubsystemHandler *handler = nullptr;

    if (_shell_callback)
    {
        handler = _shell_callback(session, channel, state->has_pty, state->winsize);
    }

    if (!handler)
    {
        SIHD_LOG(warning, "BasicSshServerHandler: shell request rejected");
        return false;
    }

    state->handler.reset(handler);
    if (!state->handler->on_start(channel, state->has_pty, state->winsize))
    {
        SIHD_LOG(error, "BasicSshServerHandler: shell handler failed to start");
        state->handler.reset();
        return false;
    }

    ++_counters.shell_requests;
    SIHD_LOG(debug, "BasicSshServerHandler: shell session started");
    return true;
}

bool BasicSshServerHandler::on_channel_request_exec([[maybe_unused]] SshServer *server,
                                                    SshSession *session,
                                                    SshChannel *channel,
                                                    std::string_view command)
{
    ChannelState *state = get_channel_state(session, channel);
    if (!state)
    {
        return false;
    }

    ISshSubsystemHandler *handler = nullptr;

    if (_exec_callback)
    {
        handler = _exec_callback(session, channel, command, state->has_pty, state->winsize);
    }

    if (!handler)
    {
        SIHD_LOG(warning, "BasicSshServerHandler: exec request rejected for command: {}", command);
        return false;
    }

    state->handler.reset(handler);
    if (!state->handler->on_start(channel, state->has_pty, state->winsize))
    {
        SIHD_LOG(error, "BasicSshServerHandler: exec handler failed to start");
        state->handler.reset();
        return false;
    }

    ++_counters.exec_requests;
    SIHD_LOG(debug, "BasicSshServerHandler: exec started: {}", command);
    return true;
}

bool BasicSshServerHandler::on_channel_request_subsystem([[maybe_unused]] SshServer *server,
                                                         SshSession *session,
                                                         SshChannel *channel,
                                                         std::string_view subsystem)
{
    ChannelState *state = get_channel_state(session, channel);
    if (!state)
    {
        return false;
    }

    ISshSubsystemHandler *handler = nullptr;

    if (_subsystem_callback)
    {
        handler = _subsystem_callback(session, channel, subsystem, state->has_pty, state->winsize);
    }

    if (!handler)
    {
        SIHD_LOG(warning, "BasicSshServerHandler: subsystem '{}' not supported", subsystem);
        return false;
    }

    state->handler.reset(handler);

    if (!state->handler->on_start(channel, state->has_pty, state->winsize))
    {
        SIHD_LOG(error, "BasicSshServerHandler: subsystem '{}' handler failed to start", subsystem);
        state->handler.reset();
        return false;
    }

    ++_counters.subsystem_requests;
    SIHD_LOG(debug, "BasicSshServerHandler: subsystem '{}' started", subsystem);
    return true;
}

void BasicSshServerHandler::on_channel_data([[maybe_unused]] SshServer *server,
                                            SshSession *session,
                                            SshChannel *channel,
                                            const void *data,
                                            size_t len,
                                            [[maybe_unused]] bool is_stderr)
{
    ChannelState *state = get_channel_state(session, channel);
    if (!state || !state->handler)
    {
        return;
    }

    ++_counters.data_received;
    _counters.bytes_received += len;
    state->handler->on_data(data, len);
}

void BasicSshServerHandler::on_channel_pty_resize([[maybe_unused]] SshServer *server,
                                                  SshSession *session,
                                                  SshChannel *channel,
                                                  const struct winsize & size)
{
    ChannelState *state = get_channel_state(session, channel);
    if (!state)
    {
        return;
    }

    state->winsize = size;

    if (state->handler)
    {
        state->handler->on_resize(size);
    }
}

bool BasicSshServerHandler::channel_bypasses_data_callback(SshChannel *channel) const
{
    // Find the channel state across all sessions
    for (const auto & [session, session_state] : _sessions)
    {
        auto it = session_state.channels.find(channel);
        if (it != session_state.channels.end())
        {
            if (it->second.handler)
            {
                return it->second.handler->manages_channel_io();
            }
            break;
        }
    }
    return false;
}

void BasicSshServerHandler::on_poll([[maybe_unused]] SshServer *server)
{
    for (auto & [session, session_state] : _sessions)
    {
        std::vector<SshChannel *> channels_to_cleanup;

        for (auto & [channel, ch_state] : session_state.channels)
        {
            if (!ch_state.handler)
            {
                continue;
            }

            if (!ch_state.handler->is_running())
            {
                // Handler finished, cleanup
                int exit_code = ch_state.handler->on_close();

                // For handlers that manage their own I/O (like SFTP),
                // libssh handles the channel close internally (via sftp_free).
                // For other handlers, we need to close the channel ourselves.
                if (!ch_state.handler->manages_channel_io())
                {
                    // Send exit status if available
                    if (exit_code >= 0)
                    {
                        channel->request_send_exit_status(exit_code);
                    }

                    // Send EOF and close channel to notify client
                    channel->send_eof();
                    channel->close();
                }

                channels_to_cleanup.push_back(channel);
                continue;
            }

            // Poll handler FDs
            poll_handler(channel, ch_state.handler.get());
        }

        // Cleanup finished handlers
        for (SshChannel *ch : channels_to_cleanup)
        {
            session_state.channels[ch].handler.reset();
        }
    }
}

// ===== Private Methods =====

BasicSshServerHandler::ChannelState *BasicSshServerHandler::get_channel_state(SshSession *session,
                                                                              SshChannel *channel)
{
    auto session_it = _sessions.find(session);
    if (session_it == _sessions.end())
    {
        return nullptr;
    }

    auto channel_it = session_it->second.channels.find(channel);
    if (channel_it == session_it->second.channels.end())
    {
        return nullptr;
    }

    return &channel_it->second;
}

void BasicSshServerHandler::poll_handler(SshChannel *channel, ISshSubsystemHandler *handler)
{
    int stdout_fd = handler->stdout_fd();
    int stderr_fd = handler->stderr_fd();

    // For handlers without FDs (like SFTP), check if they need processing
    // by calling on_data with dummy parameters - the handler will poll internally
    if (stdout_fd < 0 && stderr_fd < 0)
    {
        // Check if channel is EOF - if so, notify handler to stop
        if (channel->is_eof())
        {
            handler->on_eof();
            return;
        }
        handler->on_data(nullptr, 0);
        return;
    }

    struct pollfd pfds[2];
    int nfds = 0;

    if (stdout_fd >= 0)
    {
        pfds[nfds].fd = stdout_fd;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        ++nfds;
    }

    if (stderr_fd >= 0)
    {
        pfds[nfds].fd = stderr_fd;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        ++nfds;
    }

    // Non-blocking poll
    int ret = poll(pfds, nfds, 0);
    if (ret <= 0)
    {
        return;
    }

    char buffer[4096];
    int idx = 0;

    if (stdout_fd >= 0)
    {
        if (pfds[idx].revents & POLLIN)
        {
            ssize_t n = read(stdout_fd, buffer, sizeof(buffer));
            if (n > 0)
            {
                channel->write(sihd::util::ArrCharView(buffer, static_cast<size_t>(n)));
            }
        }
        ++idx;
    }

    if (stderr_fd >= 0 && idx < nfds)
    {
        if (pfds[idx].revents & POLLIN)
        {
            ssize_t n = read(stderr_fd, buffer, sizeof(buffer));
            if (n > 0)
            {
                channel->write_stderr(sihd::util::ArrCharView(buffer, static_cast<size_t>(n)));
            }
        }
    }
}

} // namespace sihd::ssh
