#ifndef __SIHD_SYS_IMPERSONATION_HPP__
#define __SIHD_SYS_IMPERSONATION_HPP__

#include <memory>
#include <string_view>

#include <sihd/sys/user.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

// Runs the CALLING THREAD as another account, reversibly - on both platforms only the thread
// switches (unlike user::drop_privileges: permanent, process-wide, unix only). Undone by revert()
// or the destructor. While impersonating, user::effective_user() reports the target account and
// user::real_user() the original identity; supplementary groups are not switched.
//
// The platforms differ, so each has its own entry point: windows = impersonate_with_credentials()
// (LogonUser, needs a password); linux = impersonate_as() (raw setresuid/setresgid on the thread,
// needs CAP_SETUID/CAP_SETGID); the other one is a no-op. Select on supports_credentials /
// supports_privileged.
class Impersonation
{
    public:
        // windows: LogonUser + ImpersonateLoggedOnUser
        static constexpr bool supports_credentials = sihd::util::build::is_windows;
        // linux: per-thread setresgid/setresuid
        static constexpr bool supports_privileged
            = sihd::util::build::is_linux && !sihd::util::build::is_emscripten;

        Impersonation();
        ~Impersonation();

        Impersonation(const Impersonation &) = delete;
        Impersonation & operator=(const Impersonation &) = delete;
        Impersonation(Impersonation &&) = delete;
        Impersonation & operator=(Impersonation &&) = delete;

        // authenticates the account then impersonates it - domain may be empty for a local account
        bool impersonate_with_credentials(std::string_view user_name,
                                          std::string_view password,
                                          std::string_view domain = {});

        // switches the thread identity without authenticating - requires CAP_SETUID unless the
        // target is an identity the thread already holds
        bool impersonate_as(const user::UserId & user_id, const user::GroupId & group_id);

        // restores the thread identity held when impersonation started, also done by the
        // destructor - a pre-existing impersonation is not restored
        bool revert();

        [[nodiscard]] bool impersonating() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
