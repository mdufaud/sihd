#include <sihd/sys/platform.hpp>
#include <sihd/sys/services.hpp>

#if !defined(__SIHD_WINDOWS__)
# include <netdb.h>
# include <netinet/in.h>
#else
# include <winsock2.h>

# include <ws2tcpip.h>
#endif

namespace sihd::sys
{

using namespace sihd::util;

std::optional<uint16_t> service_port(ZStringView name, ZStringView proto)
{
    struct servent *se = getservbyname(name.c_str(), proto.c_str());
    if (se == nullptr)
        return std::nullopt;
    return static_cast<uint16_t>(ntohs(static_cast<uint16_t>(se->s_port)));
}

std::optional<std::string> service_name(uint16_t port, ZStringView proto)
{
    struct servent *se = getservbyport(htons(port), proto.c_str());
    if (se == nullptr)
        return std::nullopt;
    return std::string(se->s_name);
}

} // namespace sihd::sys
