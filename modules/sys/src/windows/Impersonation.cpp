#include <windows.h>

#include <string>

#include <sihd/sys/Impersonation.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

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

        const char * c_str() const { return _str.c_str(); }

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
    if (_impl->active && !this->revert())
        SIHD_LOG(critical, "Impersonation: thread is left impersonating another account");
    _impl->close();
}

bool Impersonation::impersonate_as([[maybe_unused]] const user::UserId & user_id,
                                   [[maybe_unused]] const user::GroupId & group_id)
{
    // a token for an arbitrary account cannot be obtained without its credentials
    return false;
}

bool Impersonation::impersonate_with_credentials(std::string_view user_name,
                                                 std::string_view password,
                                                 std::string_view domain)
{
    if (_impl->active)
    {
        SIHD_LOG(error, "Impersonation: already impersonating");
        return false;
    }

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
        SIHD_LOG(error, "Impersonation: LogonUser failed for '{}': {}", user_str, os::last_error_str());
        _impl->token = nullptr;
        return false;
    }

    if (ImpersonateLoggedOnUser(_impl->token) == 0)
    {
        SIHD_LOG(error, "Impersonation: ImpersonateLoggedOnUser failed: {}", os::last_error_str());
        _impl->close();
        return false;
    }

    _impl->active = true;
    return true;
}

bool Impersonation::revert()
{
    if (!_impl->active)
        return false;
    if (RevertToSelf() == 0)
    {
        SIHD_LOG(error, "Impersonation: RevertToSelf failed: {}", os::last_error_str());
        return false;
    }
    _impl->active = false;
    _impl->close();
    return true;
}

bool Impersonation::impersonating() const
{
    return _impl->active;
}

} // namespace sihd::sys
