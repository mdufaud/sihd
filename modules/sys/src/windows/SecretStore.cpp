// clang-format off
#include <windows.h>
#include <wincred.h>
// clang-format on

#include <string>
#include <vector>

#include <fmt/format.h>

#include <sihd/sys/SecretStore.hpp>
#include <sihd/util/str.hpp>

#include "internal/win_error.hpp"

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using sihd::sys::internal::Win32Error;

namespace
{

std::wstring target_name(ZStringView service, ZStringView account)
{
    return str::to_wstr(fmt::format("{}/{}", service, account));
}

} // namespace

struct SecretStore::Impl
{
};

SecretStore::SecretStore(): _impl(std::make_unique<Impl>()) {}

SecretStore::SecretStore(SecretStore && other) = default;

SecretStore & SecretStore::operator=(SecretStore && other) = default;

SecretStore::~SecretStore() = default;

void SecretStore::set_prompt_timeout(Duration) {}

Result<void> SecretStore::store(ZStringView service, ZStringView account, std::string_view secret)
{
    auto valid = SecretStore::check_key(service, account);
    SIHD_UNEXPECTED_RETURN(valid);
    valid = SecretStore::check_secret(secret);
    SIHD_UNEXPECTED_RETURN(valid);

    std::wstring target = target_name(service, account);
    std::wstring user = str::to_wstr(account);
    std::vector<BYTE> blob(secret.begin(), secret.end());
    CREDENTIALW credential {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = target.data();
    credential.UserName = user.data();
    credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
    credential.CredentialBlob = blob.data();
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteW(&credential, 0))
    {
        const Win32Error error = Win32Error::last();
        return std::unexpected(Error(error, "could not store secret: {}", error));
    }
    return {};
}

Result<std::string> SecretStore::lookup(ZStringView service, ZStringView account)
{
    auto valid = SecretStore::check_key(service, account);
    SIHD_UNEXPECTED_RETURN(valid);

    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target_name(service, account).c_str(), CRED_TYPE_GENERIC, 0, &credential))
    {
        const Win32Error error = Win32Error::last();
        if (error.value == ERROR_NOT_FOUND)
            return std::unexpected(SecretStore::secret_not_found(service, account));
        return std::unexpected(Error(error, "could not read secret: {}", error));
    }
    std::string secret(credential->CredentialBlob, credential->CredentialBlob + credential->CredentialBlobSize);
    CredFree(credential);
    return secret;
}

Result<void> SecretStore::remove(ZStringView service, ZStringView account)
{
    auto valid = SecretStore::check_key(service, account);
    SIHD_UNEXPECTED_RETURN(valid);

    if (!CredDeleteW(target_name(service, account).c_str(), CRED_TYPE_GENERIC, 0))
    {
        const Win32Error error = Win32Error::last();
        if (error.value == ERROR_NOT_FOUND)
            return std::unexpected(SecretStore::secret_not_found(service, account));
        return std::unexpected(Error(error, "could not remove secret: {}", error));
    }
    return {};
}

} // namespace sihd::sys
