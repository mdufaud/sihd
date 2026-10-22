#include <sihd/http/IHttpAuthenticator.hpp>

namespace sihd::http
{

bool IHttpAuthenticator::on_basic_auth([[maybe_unused]] std::string_view username,
                                       [[maybe_unused]] std::string_view password)
{
    return false;
}

bool IHttpAuthenticator::on_token_auth([[maybe_unused]] std::string_view token)
{
    return false;
}

} // namespace sihd::http
