#ifndef __SIHD_SSH_SSHSESSION_HPP__
#define __SIHD_SSH_SSHSESSION_HPP__

#include <memory>
#include <string>
#include <string_view>

#include <sihd/ssh/Sftp.hpp>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshKey.hpp>
#include <sihd/ssh/SshShell.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::ssh
{

class SshSession
{
    public:
        SshSession();
        // Constructor for server-side sessions (wraps an accepted session)
        SshSession(void *session);
        virtual ~SshSession();

        sihd::util::Result<void> new_session();

        sihd::util::Result<void> set_user(std::string_view user);
        sihd::util::Result<void> set_host(std::string_view host);
        sihd::util::Result<void> set_port(int port);
        // 0=none, 1=warnings, 2=protocol, 3=packet, 4=functions
        sihd::util::Result<void> set_verbosity(int verbosity);
        void set_blocking(bool active);
        // When disabled, libssh ignores ~/.ssh/config and system ssh_config
        // (ProxyCommand/ProxyJump, etc). libssh default is enabled.
        sihd::util::Result<void> set_process_config(bool enable);
        // Override the .ssh directory libssh reads (config, known_hosts, keys)
        sihd::util::Result<void> set_ssh_dir(std::string_view path);
        // Bound connect/handshake/blocking ops (seconds). Applied by default in
        // new_session(); pass <= 0 to disable (block indefinitely).
        sihd::util::Result<void> set_timeout(int seconds);

        // Default connect/handshake timeout (seconds) set by new_session().
        static constexpr int default_timeout_sec = 10;

        sihd::util::Result<void> connect();
        bool connected();

        struct ConnectOptions
        {
                std::string_view user;
                std::string_view host;
                int port = 22;
                // 0=none, 1=warnings, 2=protocol, 3=packet, 4=functions
                int verbosity = 0;
                // false: ignore ~/.ssh/config + system ssh_config (ProxyCommand, etc)
                bool process_config = true;
                // Connect/handshake timeout (seconds); <= 0 blocks indefinitely
                int timeout_sec = default_timeout_sec;
        };
        sihd::util::Result<void> fast_connect(const ConnectOptions & options);

        sihd::util::Result<void> check_hostkey();

        struct AuthMethods
        {
                AuthMethods(int m): methods(m) {}

                bool unknown() const;
                bool none() const;
                bool password() const;
                bool public_key() const;
                bool host_based() const;
                bool interactive() const;
                bool gss_api() const;

                std::string str() const;

                int methods;
        };
        AuthMethods auth_methods();

        struct AuthState
        {
                AuthState(int st): status(st) {}

                bool error() const;
                bool denied() const;
                bool partial() const;
                bool success() const;
                bool again() const;

                const char *str() const;

                int status;
        };
        AuthState auth_none();
        AuthState auth_agent();
        AuthState auth_gssapi();
        AuthState auth_password(std::string_view password);
        AuthState auth_key(const SshKey & private_key);
        AuthState auth_key_file(std::string_view private_key_path, const char *passphrase = nullptr);
        AuthState auth_key_try(const SshKey & public_key);
        AuthState auth_key_try_file(std::string_view public_key_path);
        AuthState auth_key_auto(const char *passphrase = nullptr);
        AuthState auth_interactive_keyboard();

        void disconnect();
        void silent_disconnect();
        void delete_session();

        sihd::util::Result<void> update_known_hosts();
        sihd::util::Result<void> known_hosts(std::string & hosts);

        sihd::util::Result<void> make_channel(SshChannel & channel);
        sihd::util::Result<void> make_channel_session(SshChannel & channel);

        SshCommand make_command();
        SshShell make_shell();
        Sftp make_sftp();

        std::string get_banner();

        // Returns internal session pointer (void*)
        void *session() const;

        void set_userdata(void *userdata);
        void *userdata() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::ssh

#endif