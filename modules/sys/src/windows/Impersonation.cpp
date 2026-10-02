#include <windows.h>

#include <string>

#include <sihd/sys/Impersonation.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

using enum sihd::util::ErrorCode;
using namespace sihd::util;

namespace sihd::sys
{

SIHD_LOGGER;

namespace
{

// zeroes its copy on destruction - freed heap would keep the password
class ScrubbedString
{
    public:
        ScrubbedString(std::string_view str): _str(str) {}

        ~ScrubbedString() { SecureZeroMemory(_str.data(), _str.size()); }

        ScrubbedString(const ScrubbedString &) = delete;
        ScrubbedString & operator=(const ScrubbedString &) = delete;

        const char *c_str() const { return _str.c_str(); }

    private:
        std::string _str;
};

} // namespace

struct Impersonation::Impl
{
        HANDLE token = nullptr;
        bool active = false;

        void close()
        {
            if (token != nullptr)
            {
                CloseHandle(token);
                token = nullptr;
            }
        }
};

Impersonation::Impersonation(): _impl(std::make_unique<Impl>()) {}

Impersonation::~Impersonation()
{
    if (_impl->active)
    {
        auto reverted = this->revert();
        if (SIHD_UNEXPECTED_LOG(reverted))
            SIHD_LOG(critical, "Impersonation: thread is left impersonating another account");
    }
    _impl->close();
}

std::expected<void, Error> Impersonation::impersonate_as([[maybe_unused]] const user::UserId & user_id,
                                                         [[maybe_unused]] const user::GroupId & group_id)
{
    // a token for an arbitrary account cannot be obtained without its credentials
    return std::unexpected(Error(not_supported, "not supported on this platform"));
}

std::expected<void, Error> Impersonation::impersonate_with_credentials(std::string_view user_name,
                                                                       std::string_view password,
                                                                       std::string_view domain)
{
    if (_impl->active)
        return std::unexpected(Error(not_initialized, "already impersonating"));

    const std::string user_str(user_name);
    const ScrubbedString password_str(password);
    const std::string domain_str(domain);

    _impl->close();
    if (LogonUserA(user_str.c_str(),
                   domain_str.empty() ? nullptr : domain_str.c_str(),
                   password_str.c_str(),
                   LOGON32_LOGON_INTERACTIVE,
                   LOGON32_PROVIDER_DEFAULT,
                   &_impl->token)
        == 0)
    {
        _impl->token = nullptr;
        return std::unexpected(Error(permission_denied, "could not log user '{}': {}", user_str, os::last_error_str()));
    }

    if (ImpersonateLoggedOnUser(_impl->token) == 0)
    {
        auto error = Error(io_error, "could not impersonate: {}", os::last_error_str());
        _impl->close();
        return std::unexpected(std::move(error));
    }

    _impl->active = true;
    return {};
}

std::expected<void, Error> Impersonation::revert()
{
    if (!_impl->active)
        return std::unexpected(Error(not_initialized, "not impersonating"));
    if (RevertToSelf() == 0)
        return std::unexpected(Error(io_error, "could not revert: {}", os::last_error_str()));
    _impl->active = false;
    _impl->close();
    return {};
}

bool Impersonation::impersonating() const
{
    return _impl->active;
}

} // namespace sihd::sys
