#include <sihd/http/scheme.hpp>
#include <sihd/sys/services.hpp>

namespace sihd::http
{

bool scheme_is_ssl(std::string_view scheme)
{
    return scheme == "https" || scheme == "wss" || scheme == "ftps" || scheme == "smtps" || scheme == "imaps"
           || scheme == "pop3s";
}

int scheme_port(std::string_view scheme)
{
    if (scheme == "http" || scheme == "ws")
        return 80;
    if (scheme == "https" || scheme == "wss")
        return 443;
    return sihd::sys::service_port(scheme).value_or(0);
}

} // namespace sihd::http
