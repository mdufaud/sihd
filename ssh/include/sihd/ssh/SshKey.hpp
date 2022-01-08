#ifndef __SIHD_SSH_SSHKEY_HPP__
# define __SIHD_SSH_SSHKEY_HPP__

# include <libssh/libssh.h>
# include <string>

namespace sihd::ssh
{

class SshKey
{
    public:
        SshKey(ssh_key key = nullptr);
        virtual ~SshKey();

        static enum ssh_keytypes_e type_from_name(const char *name);
        static const char *type_to_string(enum ssh_keytypes_e type);

        bool generate(enum ssh_keytypes_e type, int parameter);
        bool import_pubkey_file(const std::string & path);
        bool import_privkey_file(const std::string & path, const char *passphrase = nullptr);
        /*
            SSH_KEYTYPE_UNKNOWN, SSH_KEYTYPE_DSS, SSH_KEYTYPE_RSA,
            SSH_KEYTYPE_RSA1, SSH_KEYTYPE_ECDSA, SSH_KEYTYPE_ED25519,
            SSH_KEYTYPE_DSS_CERT01, SSH_KEYTYPE_RSA_CERT01
        */
        bool import_pubkey_mem(const char *base64_key, enum ssh_keytypes_e type);
        /*
            SSH_KEYTYPE_UNKNOWN, SSH_KEYTYPE_DSS, SSH_KEYTYPE_RSA,
            SSH_KEYTYPE_RSA1, SSH_KEYTYPE_ECDSA, SSH_KEYTYPE_ED25519,
            SSH_KEYTYPE_DSS_CERT01, SSH_KEYTYPE_RSA_CERT01
        */
        bool import_privkey_mem(const char *base64_key, const char *passphrase = nullptr);

        bool is_equal(const SshKey & sshkey);

        const char *ecdsa_name() const;
        enum ssh_keytypes_e type() const;
        bool is_public() const;
        bool is_private() const;
        ssh_key key() const { return _ssh_key_ptr; }

        // takes ownership
        void set_key(ssh_key key);
        void clear_key();

    protected:
    
    private:
        void _new_key();

        ssh_key _ssh_key_ptr;
};

}

#endif