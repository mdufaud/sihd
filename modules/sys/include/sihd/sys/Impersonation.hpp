#ifndef __SIHD_SYS_IMPERSONATION_HPP__
#define __SIHD_SYS_IMPERSONATION_HPP__

#include <expected>
#include <memory>
#include <string_view>

#include <sihd/sys/user.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

// Runs the CALLING THREAD as another account, reversibly (revert()/dtor); drop_privileges stays
// permanent process-wide. While impersonating, effective_user() = target, real_user() = original
class Impersonation
{
    public:
        // windows: LogonUser + ImpersonateLoggedOnUser
        static constexpr bool supports_credentials = sihd::util::build::is_windows;
        // linux: per-thread setresgid/setresuid
        static constexpr bool supports_privileged = sihd::util::build::is_linux && !sihd::util::build::is_emscripten;

        Impersonation();
        ~Impersonation();

        Impersonation(const Impersonation &) = delete;
        Impersonation & operator=(const Impersonation &) = delete;
        Impersonation(Impersonation &&) = delete;
        Impersonation & operator=(Impersonation &&) = delete;

        // authenticates the account then impersonates it - domain may be empty for a local account
        std::expected<void, sihd::util::Error> impersonate_with_credentials(std::string_view user_name,
                                                                            std::string_view password,
                                                                            std::string_view domain = {});

        // switches the thread identity without authenticating - requires CAP_SETUID unless the
        // target is an identity the thread already holds
        std::expected<void, sihd::util::Error> impersonate_as(const user::UserId & user_id,
                                                              const user::GroupId & group_id);

        // restores the thread identity held when impersonation started, also done by the
        // destructor - a pre-existing impersonation is not restored
        std::expected<void, sihd::util::Error> revert();

        [[nodiscard]] bool impersonating() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
