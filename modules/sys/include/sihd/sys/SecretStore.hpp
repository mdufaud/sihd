#ifndef __SIHD_SYS_SECRETSTORE_HPP__
#define __SIHD_SYS_SECRETSTORE_HPP__

#include <memory>
#include <string>
#include <string_view>

#include <sihd/sys/desktop.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::sys
{

class SecretStore
{
    public:
        static constexpr bool supported = desktop::supported;

        // CRED_MAX_CREDENTIAL_BLOB_SIZE of the Credential Manager, kept on every platform
        static constexpr size_t max_secret_size = 2560;

        SecretStore();
        SecretStore(SecretStore && other);
        SecretStore & operator=(SecretStore && other);
        ~SecretStore();

        SecretStore(const SecretStore &) = delete;
        SecretStore & operator=(const SecretStore &) = delete;

        // how long the keyring may ask the user to unlock it, until set that is a permission_denied error
        void set_prompt_timeout(sihd::util::Duration timeout);

        // replaces the secret of that service and account
        sihd::util::Result<void>
            store(sihd::util::ZStringView service, sihd::util::ZStringView account, std::string_view secret);
        sihd::util::Result<std::string> lookup(sihd::util::ZStringView service, sihd::util::ZStringView account);
        sihd::util::Result<void> remove(sihd::util::ZStringView service, sihd::util::ZStringView account);

    private:
        struct Impl;

        static sihd::util::Result<void> check_key(sihd::util::ZStringView service, sihd::util::ZStringView account);
        static sihd::util::Result<void> check_secret(std::string_view secret);
        static sihd::util::Error secret_not_found(sihd::util::ZStringView service, sihd::util::ZStringView account);

        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
