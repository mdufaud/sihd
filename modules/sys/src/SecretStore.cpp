#include <sihd/sys/SecretStore.hpp>

namespace sihd::sys
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;

Result<void> SecretStore::check_key(ZStringView service, ZStringView account)
{
    if (service.empty() || account.empty())
        return std::unexpected(Error(invalid_argument, "a secret needs a service and an account"));
    // windows keys a credential by "service/account": a slash would make two keys collide
    if (service.view().find('/') != std::string_view::npos)
        return std::unexpected(Error(invalid_argument, "a secret service holds no '/': '{}'", service));
    return {};
}

Result<void> SecretStore::check_secret(std::string_view secret)
{
    if (secret.size() > max_secret_size)
        return std::unexpected(
            Error(invalid_argument, "a secret holds at most {} bytes, got {}", max_secret_size, secret.size()));
    return {};
}

Error SecretStore::secret_not_found(ZStringView service, ZStringView account)
{
    return Error(not_found, "no secret for account '{}' of service '{}'", account, service);
}

} // namespace sihd::sys
