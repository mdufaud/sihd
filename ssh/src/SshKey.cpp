#include <libssh/libssh.h>

#include <sihd/util/Logger.hpp>

#include <sihd/ssh/SshKey.hpp>
#include <sihd/ssh/utils.hpp>

namespace sihd::ssh
{

SIHD_NEW_LOGGER("sihd::ssh");

SshKey::SshKey(ssh_key_struct *key): _ssh_key_ptr(key)
{
    utils::init();
}

SshKey::~SshKey()
{
    this->clear_key();
    utils::finalize();
}

void SshKey::set_key(ssh_key_struct *key)
{
    this->clear_key();
    _ssh_key_ptr = key;
}

void SshKey::clear_key()
{
    if (_ssh_key_ptr != nullptr)
    {
        ssh_key_free(_ssh_key_ptr);
        _ssh_key_ptr = nullptr;
    }
}

void SshKey::_new_key()
{
    this->clear_key();
    _ssh_key_ptr = ssh_key_new();
}

bool SshKey::generate(enum ssh_keytypes_e type, int parameter)
{
    this->_new_key();
    return _ssh_key_ptr != nullptr && ssh_pki_generate(type, parameter, &_ssh_key_ptr) == SSH_OK;
}

bool SshKey::import_privkey_file(std::string_view path, const char *passphrase)
{
    this->_new_key();
    return _ssh_key_ptr != nullptr
           && ssh_pki_import_privkey_file(path.data(), passphrase, nullptr, this, &_ssh_key_ptr) == SSH_OK;
}

bool SshKey::import_privkey_mem(const char *base64_key, const char *passphrase)
{
    this->_new_key();
    return _ssh_key_ptr != nullptr
           && ssh_pki_import_privkey_base64(base64_key, passphrase, nullptr, this, &_ssh_key_ptr) == SSH_OK;
}

bool SshKey::import_pubkey_file(std::string_view path)
{
    this->_new_key();
    return _ssh_key_ptr != nullptr && ssh_pki_import_pubkey_file(path.data(), &_ssh_key_ptr) == SSH_OK;
}

bool SshKey::import_pubkey_mem(const char *base64_key, enum ssh_keytypes_e type)
{
    this->_new_key();
    return _ssh_key_ptr != nullptr && ssh_pki_import_pubkey_base64(base64_key, type, &_ssh_key_ptr) == SSH_OK;
}

bool SshKey::is_equal(const SshKey & sshkey)
{
    if (this->is_private() && sshkey.is_private())
        return ssh_key_cmp(_ssh_key_ptr, sshkey.key(), SSH_KEY_CMP_PRIVATE);
    else if (this->is_public() && sshkey.is_public())
        return ssh_key_cmp(_ssh_key_ptr, sshkey.key(), SSH_KEY_CMP_PUBLIC);
    return false;
}

const char *SshKey::ecdsa_name() const
{
    return ssh_pki_key_ecdsa_name(_ssh_key_ptr);
}

enum ssh_keytypes_e SshKey::type() const
{
    return ssh_key_type(_ssh_key_ptr);
}

bool SshKey::is_public() const
{
    return _ssh_key_ptr != nullptr && ssh_key_is_public(_ssh_key_ptr);
}

bool SshKey::is_private() const
{
    return _ssh_key_ptr != nullptr && ssh_key_is_private(_ssh_key_ptr);
}

std::string SshKey::base64() const
{
    if (_ssh_key_ptr == nullptr)
        return {};

    char *b64_key = nullptr;
    if (ssh_pki_export_pubkey_base64(_ssh_key_ptr, &b64_key) != SSH_OK)
        return {};

    std::string result(b64_key);
    ssh_string_free_char(b64_key);
    return result;
}

bool SshKey::export_privkey_file(std::string_view path, const char *passphrase) const
{
    if (_ssh_key_ptr == nullptr || !this->is_private())
    {
        SIHD_LOG(error, "SshKey: cannot export - no private key available");
        return false;
    }

    return ssh_pki_export_privkey_file(_ssh_key_ptr, passphrase, nullptr, nullptr, path.data()) == SSH_OK;
}

enum ssh_keytypes_e SshKey::type_from_name(std::string_view name)
{
    return ssh_key_type_from_name(name.data());
}

const char *SshKey::type_str(enum ssh_keytypes_e type)
{
    return ssh_key_type_to_char(type);
}

} // namespace sihd::ssh