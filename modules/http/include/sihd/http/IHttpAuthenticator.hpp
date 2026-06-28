#ifndef __SIHD_HTTP_IHTTPAUTHENTICATOR_HPP__
#define __SIHD_HTTP_IHTTPAUTHENTICATOR_HPP__

#include <string_view>

namespace sihd::http
{

class IHttpAuthenticator
{
    public:
        virtual ~IHttpAuthenticator() = default;
        // return true to allow, false to reject with 401
        virtual bool on_basic_auth([[maybe_unused]] std::string_view username,
                                   [[maybe_unused]] std::string_view password)
        {
            return false;
        }
        // return true to allow, false to reject with 401 — override to support Bearer token auth
        virtual bool on_token_auth([[maybe_unused]] std::string_view token) { return false; }
};

} // namespace sihd::http

#endif
