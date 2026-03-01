#include <memory>

#include <libssh/libssh.h>

#include <sihd/sys/LineReader.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fmt.hpp>
#include <sihd/util/str.hpp>

#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/utils.hpp>

namespace sihd::ssh
{

using namespace sihd::sys;

SIHD_LOGGER;

namespace
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

bool anon_ssh_options_set(ssh_session_struct *session,
                          const char *from,
                          ssh_options_e option,
                          const void *value)
{
    int r = ssh_options_set(session, option, value);
    if (r != SSH_OK)
        SIHD_LOG(error, "SshSession: can't set {}: {}", from, ssh_get_error(session));
    return r == SSH_OK;
}

} // namespace

struct SshSession::Impl
{
        ssh_session_struct *ssh_session_ptr {nullptr};
        bool auth_none_once {false};
        void *userdata {nullptr};
};

SshSession::SshSession(void *session): _impl_ptr(std::make_unique<Impl>())
{
    _impl_ptr->ssh_session_ptr = static_cast<ssh_session_struct *>(session);
    utils::init();
}

SshSession::SshSession(): SshSession(nullptr) {}

SshSession::~SshSession()
{
    this->delete_session();
    utils::finalize();
}

void *SshSession::session() const
{
    return _impl_ptr->ssh_session_ptr;
}

void SshSession::set_userdata(void *userdata)
{
    _impl_ptr->userdata = userdata;
}

void *SshSession::userdata() const
{
    return _impl_ptr->userdata;
}

bool SshSession::fast_connect(std::string_view user, std::string_view host, int port, int verbosity)
{
    this->delete_session();
    if (this->new_session() == false)
        return false;
    bool ret = this->set_verbosity(verbosity);
    ret = ret && this->set_user(user);
    ret = ret && this->set_host(host);
    ret = ret && this->set_port(port);
    ret = ret && this->connect();
    ret = ret && this->check_hostkey();
    return ret;
}

bool SshSession::connect()
{
    int r;
    while ((r = ssh_connect(_impl_ptr->ssh_session_ptr)) == SSH_AGAIN)
        ;
    if (r != SSH_OK)
        SIHD_LOG(error, "SshSession: connection failed: {}", ssh_get_error(_impl_ptr->ssh_session_ptr));
    return r == SSH_OK;
}

bool SshSession::connected()
{
    return _impl_ptr->ssh_session_ptr != nullptr && ssh_is_connected(_impl_ptr->ssh_session_ptr);
}

bool SshSession::check_hostkey()
{
    uint8_t *hash_ptr = nullptr;
    size_t hash_len;
    ssh_key_struct *pubkey_ptr = nullptr;

    int ret = ssh_get_server_publickey(_impl_ptr->ssh_session_ptr, &pubkey_ptr);
    if (ret == SSH_ERROR)
    {
        SIHD_LOG(error,
                 "SshSession: failed to get public key: {}",
                 ssh_get_error(_impl_ptr->ssh_session_ptr));
        return false;
    }
    SshKey pubkey(pubkey_ptr);

    ret = ssh_get_publickey_hash(pubkey_ptr, SSH_PUBLICKEY_HASH_SHA1, &hash_ptr, &hash_len);
    if (ret == SSH_ERROR)
    {
        SIHD_LOG(error,
                 "SshSession: failed to get public key sha1 hash: {}",
                 ssh_get_error(_impl_ptr->ssh_session_ptr));
        return false;
    }
    SshKeyHash hash_pubkey(hash_ptr);

    char *hexa;
#if LIBSSH_VERSION_MINOR > 7
    ssh_known_hosts_e state = ssh_session_is_known_server(_impl_ptr->ssh_session_ptr);
    switch (state)
    {
        case SSH_KNOWN_HOSTS_OK:
            return true;
        case SSH_KNOWN_HOSTS_NOT_FOUND:
        case SSH_KNOWN_HOSTS_UNKNOWN:
            // this->update_known_hosts();
            hexa = ssh_get_hexa(hash_ptr, hash_len);
            SIHD_LOG(warning, "SshSession: host key unknown: {}", hexa);
            free(hexa);
            return true;
        default:
            hexa = ssh_get_hexa(hash_ptr, hash_len);
            SIHD_LOG(error,
                     "SshSession: host key verification failed: {} (code = {})",
                     hexa,
                     static_cast<int>(state));
            break;
    }
#else
    int state = ssh_is_server_known(_impl_ptr->ssh_session_ptr);
    switch (state)
    {
        case SSH_SERVER_KNOWN_OK:
            return true;
        case SSH_SERVER_NOT_KNOWN:
        case SSH_SERVER_FILE_NOT_FOUND:
            // this->update_known_hosts();
            hexa = ssh_get_hexa(hash_ptr, hash_len);
            SIHD_LOG(warning, "SshSession: host key unknown: {}", hexa);
            free(hexa);
            return true;
        default:
            hexa = ssh_get_hexa(hash_ptr, hash_len);
            SIHD_LOG(error, "SshSession: host key verification failed: {} (code = {})", hexa, state);
            free(hexa);
            break;
    }
#endif
    return false;
}

SshSession::AuthMethods SshSession::auth_methods()
{
    if (_impl_ptr->auth_none_once == false)
        this->auth_none();
    return AuthMethods(ssh_userauth_list(_impl_ptr->ssh_session_ptr, nullptr));
}

SshSession::AuthState SshSession::auth_none()
{
    _impl_ptr->auth_none_once = true;
    return AuthState(ssh_userauth_none(_impl_ptr->ssh_session_ptr, nullptr));
}

SshSession::AuthState SshSession::auth_agent()
{
    return AuthState(ssh_userauth_agent(_impl_ptr->ssh_session_ptr, nullptr));
}

SshSession::AuthState SshSession::auth_gssapi()
{
    return AuthState(ssh_userauth_gssapi(_impl_ptr->ssh_session_ptr));
}

SshSession::AuthState SshSession::auth_password(std::string_view password)
{
    return AuthState(ssh_userauth_password(_impl_ptr->ssh_session_ptr, nullptr, password.data()));
}

SshSession::AuthState SshSession::auth_key_auto(const char *passphrase)
{
    return AuthState(ssh_userauth_publickey_auto(_impl_ptr->ssh_session_ptr, nullptr, passphrase));
}

SshSession::AuthState SshSession::auth_key_file(std::string_view private_key_path, const char *passphrase)
{
    SshKey key;

    if (key.import_privkey_file(private_key_path, passphrase))
        return this->auth_key(key);
    SIHD_LOG(error, "SshSession: failed to get private key from: {}", private_key_path);
    return AuthState(SSH_AUTH_ERROR);
}

SshSession::AuthState SshSession::auth_key_try_file(std::string_view public_key_path)
{
    SshKey key;

    if (key.import_pubkey_file(public_key_path))
        return this->auth_key_try(key);
    SIHD_LOG(error, "SshSession: failed to get public key from: {}", public_key_path);
    return AuthState(SSH_AUTH_ERROR);
}

SshSession::AuthState SshSession::auth_key(const SshKey & private_key)
{
    return AuthState(
        ssh_userauth_publickey(_impl_ptr->ssh_session_ptr, nullptr, static_cast<ssh_key>(private_key.key())));
}

SshSession::AuthState SshSession::auth_key_try(const SshKey & public_key)
{
    return AuthState(ssh_userauth_try_publickey(_impl_ptr->ssh_session_ptr,
                                                nullptr,
                                                static_cast<ssh_key>(public_key.key())));
}

SshSession::AuthState SshSession::auth_interactive_keyboard()
{
    int r = ssh_userauth_kbdint(_impl_ptr->ssh_session_ptr, NULL, NULL);
    while (r == SSH_AUTH_INFO)
    {
        const char *name = ssh_userauth_kbdint_getname(_impl_ptr->ssh_session_ptr);
        const char *instruction = ssh_userauth_kbdint_getinstruction(_impl_ptr->ssh_session_ptr);
        if (name != nullptr && name[0])
            fmt::print("{}\n", name);
        if (instruction != nullptr && instruction[0])
            fmt::print("{}\n", instruction);
        int n = ssh_userauth_kbdint_getnprompts(_impl_ptr->ssh_session_ptr);
        int i = 0;
        while (i < n)
        {
            char echo;
            const char *prompt = ssh_userauth_kbdint_getprompt(_impl_ptr->ssh_session_ptr, i, &echo);
            if (echo)
            {
                fmt::print("{}", prompt);
                std::string answer;
                sihd::sys::LineReader::fast_read_stdin(answer);
                if (ssh_userauth_kbdint_setanswer(_impl_ptr->ssh_session_ptr, i, answer.c_str()) < 0)
                    return AuthState(SSH_AUTH_ERROR);
            }
            else
            {
                char *ptr = getpass(prompt);
                bool error
                    = ptr == nullptr || ssh_userauth_kbdint_setanswer(_impl_ptr->ssh_session_ptr, i, ptr) < 0;
                if (ptr != nullptr)
                    free(ptr);
                if (error)
                    return AuthState(SSH_AUTH_ERROR);
            }
            ++i;
        }
        r = ssh_userauth_kbdint(_impl_ptr->ssh_session_ptr, NULL, NULL);
    }
    return AuthState(r);
}

bool SshSession::set_user(std::string_view user)
{
    return anon_ssh_options_set(_impl_ptr->ssh_session_ptr, "user", SSH_OPTIONS_USER, user.data());
}

bool SshSession::set_host(std::string_view host)
{
    return anon_ssh_options_set(_impl_ptr->ssh_session_ptr, "host", SSH_OPTIONS_HOST, host.data());
}

bool SshSession::set_port(int port)
{
    return anon_ssh_options_set(_impl_ptr->ssh_session_ptr, "port", SSH_OPTIONS_PORT, &port);
}

bool SshSession::set_verbosity(int verbosity)
{
    return anon_ssh_options_set(_impl_ptr->ssh_session_ptr,
                                "verbosity",
                                SSH_OPTIONS_LOG_VERBOSITY,
                                &verbosity);
}

void SshSession::set_blocking(bool active)
{
    ssh_set_blocking(_impl_ptr->ssh_session_ptr, (int)active);
}

bool SshSession::update_known_hosts()
{
#if LIBSSH_VERSION_MINOR > 7
    return ssh_session_update_known_hosts(_impl_ptr->ssh_session_ptr) == SSH_OK;
#else
    return ssh_write_knownhost(_impl_ptr->ssh_session_ptr) == SSH_OK;
#endif
}

bool SshSession::known_hosts(std::string & hosts)
{
    char *hosts_ptr;
#if LIBSSH_VERSION_MINOR > 7
    int r = ssh_session_export_known_hosts_entry(_impl_ptr->ssh_session_ptr, &hosts_ptr);
    if (r == SSH_OK)
    {
        hosts = hosts_ptr;
        ssh_string_free_char(hosts_ptr);
    }
    return r == SSH_OK;
#else
    hosts_ptr = ssh_dump_knownhost(_impl_ptr->ssh_session_ptr);
    if (hosts_ptr != nullptr)
    {
        hosts = hosts_ptr;
        ssh_string_free_char(hosts_ptr);
    }
    return hosts_ptr != nullptr;
#endif
}

void SshSession::disconnect()
{
    ssh_disconnect(_impl_ptr->ssh_session_ptr);
}

void SshSession::silent_disconnect()
{
    ssh_silent_disconnect(_impl_ptr->ssh_session_ptr);
}

bool SshSession::new_session()
{
    this->delete_session();
    _impl_ptr->ssh_session_ptr = ssh_new();
    if (_impl_ptr->ssh_session_ptr == nullptr)
        SIHD_LOG(error, "SshSession: failed to init a new ssh session");
    return _impl_ptr->ssh_session_ptr != nullptr;
}

void SshSession::delete_session()
{
    if (_impl_ptr->ssh_session_ptr != nullptr)
    {
        this->disconnect();
        ssh_free(_impl_ptr->ssh_session_ptr);
        _impl_ptr->ssh_session_ptr = nullptr;
        _impl_ptr->auth_none_once = false;
    }
}

const char *SshSession::error() const
{
    return ssh_get_error(_impl_ptr->ssh_session_ptr);
}

int SshSession::error_code() const
{
    return ssh_get_error_code(_impl_ptr->ssh_session_ptr);
}

std::string SshSession::get_banner()
{
    std::string ret;
    char *banner = ssh_get_issue_banner(_impl_ptr->ssh_session_ptr);
    if (banner != nullptr)
    {
        ret = banner;
        free(banner);
    }
    return ret;
}

SshShell SshSession::make_shell()
{
    return SshShell(_impl_ptr->ssh_session_ptr);
}

SshCommand SshSession::make_command()
{
    return SshCommand(_impl_ptr->ssh_session_ptr);
}

Sftp SshSession::make_sftp()
{
    return Sftp(_impl_ptr->ssh_session_ptr);
}

bool SshSession::make_channel(SshChannel & channel)
{
    ssh_channel channel_ptr = ssh_channel_new(_impl_ptr->ssh_session_ptr);
    if (channel_ptr == nullptr)
    {
        SIHD_LOG(error, "SshSession: failed to create a channel");
        return false;
    }
    else
        channel.set_channel(channel_ptr);
    return channel_ptr != nullptr;
}

bool SshSession::make_channel_session(SshChannel & channel)
{
    return this->make_channel(channel) && channel.open_session();
}

std::string SshSession::AuthMethods::str() const
{
    if (this->methods == 0)
        return "unknown";
    std::string ret;
    if (this->methods & SSH_AUTH_METHOD_NONE)
        sihd::util::str::append_sep(ret, "none");
    if (this->methods & SSH_AUTH_METHOD_PASSWORD)
        sihd::util::str::append_sep(ret, "password");
    if (this->methods & SSH_AUTH_METHOD_HOSTBASED)
        sihd::util::str::append_sep(ret, "hostbased");
    if (this->methods & SSH_AUTH_METHOD_INTERACTIVE)
        sihd::util::str::append_sep(ret, "keyboard-interactive");
    if (this->methods & SSH_AUTH_METHOD_GSSAPI_MIC)
        sihd::util::str::append_sep(ret, "gssapi");
    return ret;
}

bool SshSession::AuthMethods::unknown() const
{
    return methods == 0;
}

bool SshSession::AuthMethods::none() const
{
    return methods & SSH_AUTH_METHOD_NONE;
}

bool SshSession::AuthMethods::password() const
{
    return methods & SSH_AUTH_METHOD_PASSWORD;
}

bool SshSession::AuthMethods::public_key() const
{
    return methods & SSH_AUTH_METHOD_PUBLICKEY;
}

bool SshSession::AuthMethods::host_based() const
{
    return methods & SSH_AUTH_METHOD_HOSTBASED;
}

bool SshSession::AuthMethods::interactive() const
{
    return methods & SSH_AUTH_METHOD_INTERACTIVE;
}

bool SshSession::AuthMethods::gss_api() const
{
    return methods & SSH_AUTH_METHOD_GSSAPI_MIC;
}

const char *SshSession::AuthState::str() const
{
    switch (this->status)
    {
        case SSH_AUTH_ERROR:
            return "error";
        case SSH_AUTH_DENIED:
            return "denied";
        case SSH_AUTH_PARTIAL:
            return "partial";
        case SSH_AUTH_SUCCESS:
            return "success";
        case SSH_AUTH_AGAIN:
            return "again";
        default:
            return "unknown";
    }
}

bool SshSession::AuthState::error() const
{
    return status == SSH_AUTH_ERROR;
}

bool SshSession::AuthState::denied() const
{
    return status == SSH_AUTH_DENIED;
}

bool SshSession::AuthState::partial() const
{
    return status == SSH_AUTH_PARTIAL;
}

bool SshSession::AuthState::success() const
{
    return status == SSH_AUTH_SUCCESS;
}

bool SshSession::AuthState::again() const
{
    return status == SSH_AUTH_AGAIN;
}

} // namespace sihd::ssh
