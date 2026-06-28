#include <sihd/sys/services.hpp>
#include <sihd/sys/platform.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netdb.h>      // getservbyname, getservbyport
# include <netinet/in.h> // ntohs, htons
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif

namespace sihd::sys
{

std::optional<uint16_t> service_port(std::string_view name, std::string_view proto)
{
    struct servent *se = getservbyname(name.data(), proto.data());
    if (se == nullptr)
        return std::nullopt;
    return static_cast<uint16_t>(ntohs(static_cast<uint16_t>(se->s_port)));
}

std::optional<std::string> service_name(uint16_t port, std::string_view proto)
{
    struct servent *se = getservbyport(htons(port), proto.data());
    if (se == nullptr)
        return std::nullopt;
    return std::string(se->s_name);
}

} // namespace sihd::sys
