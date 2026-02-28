#include <libssh/libssh.h>

#include <sihd/util/Logger.hpp>

#include <sihd/ssh/SshKey.hpp>
#include <sihd/ssh/utils.hpp>

namespace sihd::ssh
{

SIHD_NEW_LOGGER("sihd::ssh");

namespace
{

enum ssh_keytypes_e to_libssh(KeyType type)
{
    return static_cast<enum ssh_keytypes_e>(static_cast<int>(type));
}

KeyType from_libssh(enum ssh_keytypes_e type)
{
    return static_cast<KeyType>(static_cast<int>(type));
}

} // namespace

struct SshKey::Impl
{
        ssh_key_struct *ssh_key_ptr {nullptr};
};

SshKey::SshKey(void *key): _impl_ptr(std::make_unique<Impl>())
{
    _impl_ptr->ssh_key_ptr = static_cast<ssh_key_struct *>(key);
    utils::init();
}

SshKey::~SshKey()
{
    this->clear_key();
    utils::finalize();
}

void SshKey::set_key(void *key)
{
    this->clear_key();
    _impl_ptr->ssh_key_ptr = static_cast<ssh_key_struct *>(key);
}

void SshKey::clear_key()
{
    if (_impl_ptr->ssh_key_ptr != nullptr)
    {
        ssh_key_free(_impl_ptr->ssh_key_ptr);
        _impl_ptr->ssh_key_ptr = nullptr;
    }
}

void *SshKey::key() const
{
    return _impl_ptr->ssh_key_ptr;
}

bool SshKey::generate(KeyType type, int parameter)
{
    this->clear_key();
    _impl_ptr->ssh_key_ptr = ssh_key_new();
    return _impl_ptr->ssh_key_ptr != nullptr
           && ssh_pki_generate(to_libssh(type), parameter, &_impl_ptr->ssh_key_ptr) == SSH_OK;
}

bool SshKey::import_privkey_file(std::string_view path, const char *passphrase)
{
    this->clear_key();
    _impl_ptr->ssh_key_ptr = ssh_key_new();
    return _impl_ptr->ssh_key_ptr != nullptr
           && ssh_pki_import_privkey_file(path.data(), passphrase, nullptr, this, &_impl_ptr->ssh_key_ptr)
                  == SSH_OK;
}

bool SshKey::import_privkey_mem(const char *base64_key, const char *passphrase)
{
    this->clear_key();
    _impl_ptr->ssh_key_ptr = ssh_key_new();
    return _impl_ptr->ssh_key_ptr != nullptr
           && ssh_pki_import_privkey_base64(base64_key, passphrase, nullptr, this, &_impl_ptr->ssh_key_ptr)
                  == SSH_OK;
}

bool SshKey::import_pubkey_file(std::string_view path)
{
    this->clear_key();
    _impl_ptr->ssh_key_ptr = ssh_key_new();
    return _impl_ptr->ssh_key_ptr != nullptr
           && ssh_pki_import_pubkey_file(path.data(), &_impl_ptr->ssh_key_ptr) == SSH_OK;
}

bool SshKey::import_pubkey_mem(const char *base64_key, KeyType type)
{
    this->clear_key();
    _impl_ptr->ssh_key_ptr = ssh_key_new();
    return _impl_ptr->ssh_key_ptr != nullptr
           && ssh_pki_import_pubkey_base64(base64_key, to_libssh(type), &_impl_ptr->ssh_key_ptr) == SSH_OK;
}

bool SshKey::is_equal(const SshKey & sshkey)
{
    auto *other_key = static_cast<ssh_key_struct *>(sshkey.key());
    if (this->is_private() && sshkey.is_private())
        return ssh_key_cmp(_impl_ptr->ssh_key_ptr, other_key, SSH_KEY_CMP_PRIVATE);
    else if (this->is_public() && sshkey.is_public())
        return ssh_key_cmp(_impl_ptr->ssh_key_ptr, other_key, SSH_KEY_CMP_PUBLIC);
    return false;
}

const char *SshKey::ecdsa_name() const
{
    return ssh_pki_key_ecdsa_name(_impl_ptr->ssh_key_ptr);
}

KeyType SshKey::type() const
{
    return from_libssh(ssh_key_type(_impl_ptr->ssh_key_ptr));
}

bool SshKey::is_public() const
{
    return _impl_ptr->ssh_key_ptr != nullptr && ssh_key_is_public(_impl_ptr->ssh_key_ptr);
}

bool SshKey::is_private() const
{
    return _impl_ptr->ssh_key_ptr != nullptr && ssh_key_is_private(_impl_ptr->ssh_key_ptr);
}

std::string SshKey::base64() const
{
    if (_impl_ptr->ssh_key_ptr == nullptr)
        return {};

    char *b64_key = nullptr;
    if (ssh_pki_export_pubkey_base64(_impl_ptr->ssh_key_ptr, &b64_key) != SSH_OK)
        return {};

    std::string result(b64_key);
    ssh_string_free_char(b64_key);
    return result;
}

bool SshKey::export_privkey_file(std::string_view path, const char *passphrase) const
{
    if (_impl_ptr->ssh_key_ptr == nullptr || !this->is_private())
    {
        SIHD_LOG(error, "SshKey: cannot export - no private key available");
        return false;
    }

    return ssh_pki_export_privkey_file(_impl_ptr->ssh_key_ptr, passphrase, nullptr, nullptr, path.data())
           == SSH_OK;
}

KeyType SshKey::type_from_name(std::string_view name)
{
    return from_libssh(ssh_key_type_from_name(name.data()));
}

const char *SshKey::type_str(KeyType type)
{
    return ssh_key_type_to_char(to_libssh(type));
}

} // namespace sihd::ssh