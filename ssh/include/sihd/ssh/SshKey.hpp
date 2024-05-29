#ifndef __SIHD_SSH_SSHKEY_HPP__
#define __SIHD_SSH_SSHKEY_HPP__

#include <string>

#pragma message("TODO forward enum")
#include <libssh/libssh.h>

struct ssh_key_struct;

namespace sihd::ssh
{

class SshKey
{
    public:
        SshKey(ssh_key_struct *key = nullptr);
        virtual ~SshKey();

        static enum ssh_keytypes_e type_from_name(std::string_view name);
        static const char *type_str(enum ssh_keytypes_e type);

        bool generate(enum ssh_keytypes_e type, int parameter);
        bool import_pubkey_file(std::string_view path);
        bool import_privkey_file(std::string_view path, const char *passphrase = nullptr);
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
        ssh_key_struct *key() const { return _ssh_key_ptr; }

        // takes ownership
        void set_key(ssh_key_struct *key);
        void clear_key();

    protected:

    private:
        void _new_key();

        ssh_key_struct *_ssh_key_ptr;
};

} // namespace sihd::ssh

#endif