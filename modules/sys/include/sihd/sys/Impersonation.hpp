#ifndef __SIHD_SYS_IMPERSONATION_HPP__
#define __SIHD_SYS_IMPERSONATION_HPP__

#include <memory>
#include <string_view>

#include <sihd/sys/user.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>
#include <sihd/util/build.hpp>

namespace sihd::sys
{

// impersonates the CALLING THREAD only, reversibly: effective_user() = target, real_user() = original meanwhile
class Impersonation
{
    public:
        static constexpr bool supports_credentials = sihd::util::build::is_windows;
        static constexpr bool supports_privileged = sihd::util::build::is_linux && !sihd::util::build::is_emscripten;

        Impersonation();
        ~Impersonation();

        Impersonation(const Impersonation &) = delete;
        Impersonation & operator=(const Impersonation &) = delete;
        Impersonation(Impersonation &&) = delete;
        Impersonation & operator=(Impersonation &&) = delete;

        // authenticates then impersonates; domain may be empty for a local account
        sihd::util::Result<void> impersonate_with_credentials(sihd::util::ZStringView user_name,
                                                              std::string_view password,
                                                              sihd::util::ZStringView domain = "");

        // switches the thread identity without authenticating: needs CAP_SETUID unless the target is already held
        sihd::util::Result<void> impersonate_as(const user::UserId & user_id, const user::GroupId & group_id);

        // restores the identity held at impersonation start (also done by the dtor); a prior one is not restored
        sihd::util::Result<void> revert();

        [[nodiscard]] bool impersonating() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::sys

#endif
