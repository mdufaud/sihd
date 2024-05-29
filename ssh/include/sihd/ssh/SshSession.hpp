#ifndef __SIHD_SSH_SSHSESSION_HPP__
#define __SIHD_SSH_SSHSESSION_HPP__

#include <sihd/ssh/Sftp.hpp>
#include <sihd/ssh/SshChannel.hpp>
#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshKey.hpp>
#include <sihd/ssh/SshScp.hpp>
#include <sihd/ssh/SshShell.hpp>

namespace sihd::ssh
{

class SshSession
{
    public:
        SshSession();
        virtual ~SshSession();

        bool new_session();

        bool set_user(std::string_view user);
        bool set_host(std::string_view host);
        bool set_port(int port);
        /**
         * No logging at all = 0;
         * Only warnings = 1;
         * High level protocol information = 2;
         * Lower level protocol infomations, packet level = 3;
         * Every function path = 4;
         */
        bool set_verbosity(int verbosity);
        void set_blocking(bool active);

        bool connect();
        bool connected();
        bool fast_connect(std::string_view user, std::string_view host, int port = 22, int verbosity = SSH_LOG_NOLOG);

        bool check_hostkey();

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

        bool update_known_hosts();
        bool known_hosts(std::string & hosts);

        bool make_channel(SshChannel & channel);
        bool make_channel_session(SshChannel & channel);

        SshScp make_scp();
        SshCommand make_command();
        SshShell make_shell();
        Sftp make_sftp();

        std::string get_banner();

        ssh_session_struct *session() const { return _ssh_session_ptr; }
        const char *error() const;
        int error_code() const;

    protected:

    private:
        bool _init_scp(SshScp & scp);

        ssh_session_struct *_ssh_session_ptr;
        bool _auth_none_once;
};

} // namespace sihd::ssh

#endif