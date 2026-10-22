#include <memory>

#include <libssh/libssh.h>

#include <sihd/ssh/SshSession.hpp>
#include <sihd/ssh/utils.hpp>
#include <sihd/sys/LineReader.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>

#if defined(__SIHD_WINDOWS__)
# include <conio.h>
#endif

#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/fmt.hpp>
#include <sihd/util/str.hpp>

#include "ssh_error.hpp"

using namespace sihd::sys;
using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::ssh
{

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

struct SshKeyFree
{
        void operator()(ssh_key_struct *key) const { ssh_key_free(key); }
};

using SshKeyPtr = std::unique_ptr<ssh_key_struct, SshKeyFree>;

Result<void> set_ssh_option(ssh_session_struct *session, std::string_view name, ssh_options_e option, const void *value)
{
    if (ssh_options_set(session, option, value) == SSH_OK)
        return {};
    return std::unexpected(Error(session, "could not set '{}' option: {}", name, ssh_error_str(session)));
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
    SIHD_UNEXPECTED_LOG(utils::init());
}

SshSession::SshSession(): SshSession(nullptr) {}

SshSession::~SshSession()
{
    this->delete_session();
    SIHD_UNEXPECTED_LOG(utils::finalize());
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

Result<void> SshSession::fast_connect(const SshSession::ConnectOptions & options)
{
    this->delete_session();
    if (auto res = this->new_session(); !res)
        return res;
    if (auto res = this->set_verbosity(options.verbosity); !res)
        return res;
    if (auto res = this->set_user(std::string(options.user)); !res)
        return res;
    if (auto res = this->set_host(std::string(options.host)); !res)
        return res;
    if (auto res = this->set_port(options.port); !res)
        return res;
    if (auto res = this->set_timeout(options.timeout_sec); !res)
        return res;
    if (!options.process_config)
    {
        if (auto res = this->set_process_config(false); !res)
            return res;
    }
    if (auto res = this->connect(); !res)
        return res;
    return this->check_hostkey();
}

Result<void> SshSession::connect()
{
    int status = 0;
    while ((status = ssh_connect(_impl_ptr->ssh_session_ptr)) == SSH_AGAIN)
        ;
    if (status == SSH_OK)
        return {};
    return std::unexpected(
        Error(_impl_ptr->ssh_session_ptr, "could not connect: {}", ssh_error_str(_impl_ptr->ssh_session_ptr)));
}

bool SshSession::connected()
{
    return _impl_ptr->ssh_session_ptr != nullptr && ssh_is_connected(_impl_ptr->ssh_session_ptr);
}

Result<void> SshSession::check_hostkey()
{
    size_t hash_len = 0;
    SshKeyPtr pubkey;
    SshKeyHash hash_pubkey;

    if (ssh_get_server_publickey(_impl_ptr->ssh_session_ptr, std::out_ptr(pubkey)) == SSH_ERROR)
        return std::unexpected(Error(_impl_ptr->ssh_session_ptr,
                                     "could not get server public key: {}",
                                     ssh_error_str(_impl_ptr->ssh_session_ptr)));

    if (ssh_get_publickey_hash(pubkey.get(), SSH_PUBLICKEY_HASH_SHA1, std::out_ptr(hash_pubkey), &hash_len)
        == SSH_ERROR)
        return std::unexpected(Error(_impl_ptr->ssh_session_ptr,
                                     "could not get public key sha1 hash: {}",
                                     ssh_error_str(_impl_ptr->ssh_session_ptr)));

#if LIBSSH_VERSION_MINOR > 7
    const ssh_known_hosts_e state = ssh_session_is_known_server(_impl_ptr->ssh_session_ptr);
    switch (state)
    {
        case SSH_KNOWN_HOSTS_OK:
            return {};
        case SSH_KNOWN_HOSTS_NOT_FOUND:
        case SSH_KNOWN_HOSTS_UNKNOWN:
            // intentional: accept unknown host with a warning (local-server use)
            {
                char *hexa = ssh_get_hexa(hash_pubkey.get(), hash_len);
                SIHD_LOG(warning, "host key unknown: {}", hexa);
                free(hexa);
            }
            return {};
        default:
        {
            char *hexa = ssh_get_hexa(hash_pubkey.get(), hash_len);
            Error err(_impl_ptr->ssh_session_ptr,
                      "host key verification failed: {} (state = {}): {}",
                      hexa,
                      static_cast<int>(state),
                      ssh_error_str(_impl_ptr->ssh_session_ptr));
            free(hexa);
            return std::unexpected(std::move(err));
        }
    }
#else
    const int state = ssh_is_server_known(_impl_ptr->ssh_session_ptr);
    switch (state)
    {
        case SSH_SERVER_KNOWN_OK:
            return {};
        case SSH_SERVER_NOT_KNOWN:
        case SSH_SERVER_FILE_NOT_FOUND:
            // intentional: accept unknown host with a warning (local-server use)
            {
                char *hexa = ssh_get_hexa(hash_pubkey.get(), hash_len);
                SIHD_LOG(warning, "host key unknown: {}", hexa);
                free(hexa);
            }
            return {};
        default:
        {
            char *hexa = ssh_get_hexa(hash_pubkey.get(), hash_len);
            Error err(_impl_ptr->ssh_session_ptr,
                      "host key verification failed: {} (state = {}): {}",
                      hexa,
                      state,
                      ssh_error_str(_impl_ptr->ssh_session_ptr));
            free(hexa);
            return std::unexpected(std::move(err));
        }
    }
#endif
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
#if !defined(__SIHD_WINDOWS__)
    return AuthState(ssh_userauth_agent(_impl_ptr->ssh_session_ptr, nullptr));
#else
    SIHD_LOG(warning, "ssh-agent auth not supported on Windows");
    return AuthState(SSH_AUTH_DENIED);
#endif
}

SshSession::AuthState SshSession::auth_gssapi()
{
    return AuthState(ssh_userauth_gssapi(_impl_ptr->ssh_session_ptr));
}

SshSession::AuthState SshSession::auth_password(const std::string & password)
{
    return AuthState(ssh_userauth_password(_impl_ptr->ssh_session_ptr, nullptr, password.c_str()));
}

SshSession::AuthState SshSession::auth_key_auto(const char *passphrase)
{
    return AuthState(ssh_userauth_publickey_auto(_impl_ptr->ssh_session_ptr, nullptr, passphrase));
}

SshSession::AuthState SshSession::auth_key_file(const std::string & private_key_path, const char *passphrase)
{
    SshKey key;

    if (SIHD_UNEXPECTED_LOG(key.import_privkey_file(private_key_path, passphrase)))
        return AuthState(SSH_AUTH_ERROR);
    return this->auth_key(key);
}

SshSession::AuthState SshSession::auth_key_try_file(const std::string & public_key_path)
{
    SshKey key;

    if (SIHD_UNEXPECTED_LOG(key.import_pubkey_file(public_key_path)))
        return AuthState(SSH_AUTH_ERROR);
    return this->auth_key_try(key);
}

SshSession::AuthState SshSession::auth_key(const SshKey & private_key)
{
    return AuthState(
        ssh_userauth_publickey(_impl_ptr->ssh_session_ptr, nullptr, static_cast<ssh_key>(private_key.key())));
}

SshSession::AuthState SshSession::auth_key_try(const SshKey & public_key)
{
    return AuthState(
        ssh_userauth_try_publickey(_impl_ptr->ssh_session_ptr, nullptr, static_cast<ssh_key>(public_key.key())));
}

SshSession::AuthState SshSession::auth_interactive_keyboard()
{
    int status = ssh_userauth_kbdint(_impl_ptr->ssh_session_ptr, NULL, NULL);
    while (status == SSH_AUTH_INFO)
    {
        const char *name = ssh_userauth_kbdint_getname(_impl_ptr->ssh_session_ptr);
        const char *instruction = ssh_userauth_kbdint_getinstruction(_impl_ptr->ssh_session_ptr);
        if (name != nullptr && name[0])
            fmt::print("{}\n", name);
        if (instruction != nullptr && instruction[0])
            fmt::print("{}\n", instruction);
        int nprompts = ssh_userauth_kbdint_getnprompts(_impl_ptr->ssh_session_ptr);
        int i = 0;
        while (i < nprompts)
        {
            char echo;
            const char *prompt = ssh_userauth_kbdint_getprompt(_impl_ptr->ssh_session_ptr, i, &echo);

            if (echo)
            {
                fmt::print("{}", prompt);

                auto answer = sihd::sys::LineReader::fast_read_stdin();
                if (SIHD_UNEXPECTED_LOG(answer))
                    return AuthState(SSH_AUTH_ERROR);
                if (ssh_userauth_kbdint_setanswer(_impl_ptr->ssh_session_ptr, i, answer->c_str()) < 0)
                    return AuthState(SSH_AUTH_ERROR);
            }
            else
            {
#if !defined(__SIHD_WINDOWS__)
                char *ptr = getpass(prompt);
                bool error = ptr == nullptr || ssh_userauth_kbdint_setanswer(_impl_ptr->ssh_session_ptr, i, ptr) < 0;
                if (ptr != nullptr)
                    free(ptr);
                if (error)
                    return AuthState(SSH_AUTH_ERROR);
#else
                fmt::print("{}", prompt);
                std::string answer;
                int key;
                while ((key = _getch()) != '\r' && key != '\n')
                {
                    if (key == '\b' && !answer.empty())
                        answer.pop_back();
                    else
                        answer += static_cast<char>(key);
                }
                fmt::print("\n");
                if (ssh_userauth_kbdint_setanswer(_impl_ptr->ssh_session_ptr, i, answer.c_str()) < 0)
                    return AuthState(SSH_AUTH_ERROR);
#endif
            }
            ++i;
        }
        status = ssh_userauth_kbdint(_impl_ptr->ssh_session_ptr, NULL, NULL);
    }
    return AuthState(status);
}

Result<void> SshSession::set_user(const std::string & user)
{
    return set_ssh_option(_impl_ptr->ssh_session_ptr, "user", SSH_OPTIONS_USER, user.c_str());
}

Result<void> SshSession::set_host(const std::string & host)
{
    return set_ssh_option(_impl_ptr->ssh_session_ptr, "host", SSH_OPTIONS_HOST, host.c_str());
}

Result<void> SshSession::set_port(int port)
{
    return set_ssh_option(_impl_ptr->ssh_session_ptr, "port", SSH_OPTIONS_PORT, &port);
}

Result<void> SshSession::set_verbosity(int verbosity)
{
    return set_ssh_option(_impl_ptr->ssh_session_ptr, "verbosity", SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
}

Result<void> SshSession::set_process_config(bool enable)
{
    return set_ssh_option(_impl_ptr->ssh_session_ptr, "process_config", SSH_OPTIONS_PROCESS_CONFIG, &enable);
}

Result<void> SshSession::set_ssh_dir(const std::string & path)
{
    return set_ssh_option(_impl_ptr->ssh_session_ptr, "ssh_dir", SSH_OPTIONS_SSH_DIR, path.c_str());
}

Result<void> SshSession::set_timeout(int seconds)
{
    long value = seconds > 0 ? seconds : 0;
    return set_ssh_option(_impl_ptr->ssh_session_ptr, "timeout", SSH_OPTIONS_TIMEOUT, &value);
}

void SshSession::set_blocking(bool active)
{
    ssh_set_blocking(_impl_ptr->ssh_session_ptr, (int)active);
}

Result<void> SshSession::update_known_hosts()
{
    const int status =
#if LIBSSH_VERSION_MINOR > 7
        ssh_session_update_known_hosts(_impl_ptr->ssh_session_ptr);
#else
        ssh_write_knownhost(_impl_ptr->ssh_session_ptr);
#endif
    if (status == SSH_OK)
        return {};
    return std::unexpected(Error(_impl_ptr->ssh_session_ptr,
                                 "could not update known hosts: {}",
                                 ssh_error_str(_impl_ptr->ssh_session_ptr)));
}

Result<void> SshSession::known_hosts(std::string & hosts)
{
#if LIBSSH_VERSION_MINOR > 7
    char *hosts_ptr = nullptr;
    if (ssh_session_export_known_hosts_entry(_impl_ptr->ssh_session_ptr, &hosts_ptr) != SSH_OK)
        return std::unexpected(Error(_impl_ptr->ssh_session_ptr,
                                     "could not export known hosts entry: {}",
                                     ssh_error_str(_impl_ptr->ssh_session_ptr)));
#else
    char *hosts_ptr = ssh_dump_knownhost(_impl_ptr->ssh_session_ptr);
    if (hosts_ptr == nullptr)
        return std::unexpected(Error(_impl_ptr->ssh_session_ptr,
                                     "could not dump known hosts entry: {}",
                                     ssh_error_str(_impl_ptr->ssh_session_ptr)));
#endif
    hosts = hosts_ptr;
    ssh_string_free_char(hosts_ptr);
    return {};
}

void SshSession::disconnect()
{
    ssh_disconnect(_impl_ptr->ssh_session_ptr);
}

void SshSession::silent_disconnect()
{
    ssh_silent_disconnect(_impl_ptr->ssh_session_ptr);
}

Result<void> SshSession::new_session()
{
    this->delete_session();
    _impl_ptr->ssh_session_ptr = ssh_new();
    if (_impl_ptr->ssh_session_ptr == nullptr)
        return std::unexpected(Error(unknown, "could not init a new ssh session"));
    return this->set_timeout(default_timeout_sec);
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

Result<void> SshSession::make_channel(SshChannel & channel)
{
    ssh_channel channel_ptr = ssh_channel_new(_impl_ptr->ssh_session_ptr);
    if (channel_ptr == nullptr)
        return std::unexpected(Error(_impl_ptr->ssh_session_ptr,
                                     "could not create a channel: {}",
                                     ssh_error_str(_impl_ptr->ssh_session_ptr)));
    channel.set_channel(channel_ptr);
    return {};
}

Result<void> SshSession::make_channel_session(SshChannel & channel)
{
    if (auto res = this->make_channel(channel); !res)
        return res;
    return channel.open_session();
}

std::string SshSession::AuthMethods::str() const
{
    if (this->methods == 0)
        return "unknown";
    std::string ret;
    if (this->methods & SSH_AUTH_METHOD_NONE)
        str::append_sep(ret, "none");
    if (this->methods & SSH_AUTH_METHOD_PASSWORD)
        str::append_sep(ret, "password");
    if (this->methods & SSH_AUTH_METHOD_HOSTBASED)
        str::append_sep(ret, "hostbased");
    if (this->methods & SSH_AUTH_METHOD_INTERACTIVE)
        str::append_sep(ret, "keyboard-interactive");
    if (this->methods & SSH_AUTH_METHOD_GSSAPI_MIC)
        str::append_sep(ret, "gssapi");
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

SshSession::AuthMethods::AuthMethods(int m): methods(m) {}

SshSession::AuthState::AuthState(int st): status(st) {}

} // namespace sihd::ssh
