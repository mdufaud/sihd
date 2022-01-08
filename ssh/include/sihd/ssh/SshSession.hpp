#ifndef __SIHD_SSH_SSHSESSION_HPP__
# define __SIHD_SSH_SSHSESSION_HPP__

# include <memory>
# include <libssh/libssh.h>
# include <sihd/ssh/SshKey.hpp>
# include <sihd/ssh/SshChannel.hpp>
# include <sihd/ssh/SshScp.hpp>
# include <sihd/ssh/SshCommand.hpp>
# include <sihd/ssh/SshFtp.hpp>

namespace sihd::ssh
{

struct SshKeyHashDeleter
{
    void operator()(uint8_t *ptr)
    {
        if (ptr != nullptr)
            ssh_clean_pubkey_hash(&ptr);
    }
};

using SshKeyHash = std::unique_ptr<uint8_t, SshKeyHashDeleter>;

class SshSession
{
    public:
        SshSession();
        virtual ~SshSession();

        bool new_session();

        bool set_user(const std::string & user);
        bool set_host(const std::string & host);
        bool set_port(int port);
        // verbosity = SSH_LOG_NOLOG, SSH_LOG_WARNING, SSH_LOG_PROTOCOL, SSH_LOG_PACKET, SSH_LOG_FUNCTIONS
        bool set_verbosity(int verbosity);
        void set_blocking(bool active);

        bool connect();
        bool connected();
        bool fast_connect(const std::string & user, const std::string & host, int port = 22);

        bool check_hostkey();

        struct AuthMethods
        {
            AuthMethods(int m): methods(m) {}

            bool unknown() const { return methods == 0; }
            bool none() const { return methods & SSH_AUTH_METHOD_NONE; }
            bool password() const { return methods & SSH_AUTH_METHOD_PASSWORD; }
            bool public_key() const { return methods & SSH_AUTH_METHOD_PUBLICKEY; }
            bool host_based() const { return methods & SSH_AUTH_METHOD_HOSTBASED; }
            bool interactive() const { return methods & SSH_AUTH_METHOD_INTERACTIVE; }
            bool gss_api() const { return methods & SSH_AUTH_METHOD_GSSAPI_MIC; }

            std::string to_string() const;

            int methods;
        };
        AuthMethods auth_methods();

        struct AuthState
        {
            AuthState(int st): status(st) {}

            bool error() const { return status == SSH_AUTH_ERROR; }
            bool denied() const { return status == SSH_AUTH_DENIED; }
            bool partial() const { return status == SSH_AUTH_PARTIAL; }
            bool success() const { return status == SSH_AUTH_SUCCESS; }
            bool again() const { return status == SSH_AUTH_AGAIN; }

            std::string to_string() const;

            int status;
        };
        AuthState auth_none();
        AuthState auth_agent();
        AuthState auth_gssapi();
        AuthState auth_password(const std::string & password);
        AuthState auth_key(const SshKey & private_key);
        AuthState auth_key_file(const std::string & private_key_path, const char *passphrase = nullptr);
        AuthState auth_key_try(const SshKey & public_key);
        AuthState auth_key_try_file(const std::string & public_key_path);
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

        std::string get_banner();

        ssh_session session() const { return _ssh_session_ptr; }
        const char *error() const;
        int error_code() const;

    protected:
    
    private:
        bool _set(const char *from, ssh_options_e option, const void *value);
        bool _init_scp(SshScp & scp);

        ssh_session _ssh_session_ptr;
        bool _auth_none_once;
};

}

#endif