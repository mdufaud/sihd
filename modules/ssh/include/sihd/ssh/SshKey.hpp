#ifndef __SIHD_SSH_SSHKEY_HPP__
#define __SIHD_SSH_SSHKEY_HPP__

#include <cstdint>
#include <memory>
#include <string>

#include <sihd/util/Result.hpp>

namespace sihd::ssh
{

// Mirrors libssh ssh_keytypes_e
enum class KeyType : int
{
    Unknown = 0,
    Dss = 1,
    Rsa = 2,
    Rsa1 = 3,
    Ecdsa = 4,
    Ed25519 = 5,
    DssCert01 = 6,
    RsaCert01 = 7,
};

class SshKey
{
    public:
        SshKey(void *key = nullptr);
        ~SshKey();

        static KeyType type_from_name(const std::string & name);
        static const char *type_str(KeyType type);

        sihd::util::Result<void> generate(KeyType type, int parameter);
        sihd::util::Result<void> import_pubkey_file(const std::string & path);
        sihd::util::Result<void> import_privkey_file(const std::string & path, const char *passphrase = nullptr);
        sihd::util::Result<void> import_pubkey_mem(const char *base64_key, KeyType type = KeyType::Unknown);
        sihd::util::Result<void> import_privkey_mem(const char *base64_key, const char *passphrase = nullptr);

        bool is_equal(const SshKey & sshkey);

        const char *ecdsa_name() const;
        KeyType type() const;
        bool is_public() const;
        bool is_private() const;

        void *key() const;

        std::string base64() const;
        sihd::util::Result<void> export_privkey_file(const std::string & path, const char *passphrase = nullptr) const;

        // Takes ownership
        void set_key(void *key);
        void clear_key();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl_ptr;
};

} // namespace sihd::ssh

#endif