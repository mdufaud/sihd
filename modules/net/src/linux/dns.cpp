#include <sihd/util/build.hpp>

#if defined(__SIHD_LINUX__)

# include <netinet/in.h>

# include <algorithm>
# include <optional>

# include <sihd/net/dns.hpp>
# include <sihd/net/ip.hpp>
# include <sihd/sys/fs.hpp>
# include <sihd/sys/linux/dbus.hpp>
# include <sihd/util/Error.hpp>
# include <sihd/util/str.hpp>

# include "interfaces.hpp"

using namespace sihd::util;
using namespace sihd::sys::linux_;

namespace sihd::net::dns
{

namespace
{

constexpr ZStringView resolv_conf_path = "/etc/resolv.conf";
constexpr ZStringView resolved_service = "org.freedesktop.resolve1";
constexpr ZStringView resolved_path = "/org/freedesktop/resolve1";
constexpr ZStringView properties_interface = "org.freedesktop.DBus.Properties";
constexpr ZStringView resolved_manager_interface = "org.freedesktop.resolve1.Manager";

std::optional<DnsServer> parse_nameserver(const std::string & value)
{
    const size_t percent = value.find('%');
    const std::string address = value.substr(0, percent);
    const std::string scope = percent == std::string::npos ? "" : value.substr(percent + 1);

    if (ip::is_valid_ipv4(address))
        return DnsServer {.addr = IpAddr(address), .interface_name = scope};
    if (!ip::is_valid_ipv6(address))
        return std::nullopt;

    sockaddr_in6 ipv6 = IpAddr(address).addr6();
    if (!scope.empty())
        ipv6.sin6_scope_id = interfaces::index_of(scope).value_or(0);
    return DnsServer {.addr = IpAddr(ipv6), .interface_name = scope};
}

std::vector<DnsServer> parse_resolv_conf(std::string_view content)
{
    std::vector<DnsServer> servers;
    for (const std::string & line : str::split(content, '\n'))
    {
        const std::vector<std::string> words = str::split(line);
        if (words.size() < 2 || words[0] != "nameserver")
            continue;
        if (auto server = parse_nameserver(words[1]))
            servers.push_back(std::move(*server));
    }
    return servers;
}

// systemd-resolved puts its own stub in resolv.conf and keeps the real servers to itself
bool is_resolved_stub(const DnsServer & server)
{
    const std::string address = server.addr.str();
    return address == "127.0.0.53" || address == "127.0.0.54";
}

// Manager.DNS is a(iiay): interface index (0 = global), address family, address bytes
Result<std::vector<DnsServer>> resolved_servers(Duration timeout)
{
    dbus::Connection connection;
    auto opened = connection.open(dbus::Bus::system);
    SIHD_UNEXPECTED_RETURN(opened);
    auto request = dbus::Message::method_call(resolved_service, resolved_path, properties_interface, "Get");
    SIHD_UNEXPECTED_RETURN(request);
    auto added_interface = request->add(resolved_manager_interface);
    SIHD_UNEXPECTED_RETURN(added_interface);
    auto added_property = request->add("DNS");
    SIHD_UNEXPECTED_RETURN(added_property);
    auto reply = request->call(connection, timeout);
    SIHD_UNEXPECTED_RETURN(reply);

    auto array = dbus::Iter(*reply).recurse();
    SIHD_UNEXPECTED_RETURN(array);
    auto entries = array->recurse();
    SIHD_UNEXPECTED_RETURN(entries);

    std::vector<DnsServer> servers;
    for (; entries->type() != '\0'; entries->next())
    {
        auto entry = entries->recurse();
        SIHD_UNEXPECTED_RETURN(entry);
        auto index = entry->get<int32_t>();
        SIHD_UNEXPECTED_RETURN(index);
        entry->next();
        auto family = entry->get<int32_t>();
        SIHD_UNEXPECTED_RETURN(family);
        entry->next();
        auto bytes = entry->get_array<uint8_t>();
        SIHD_UNEXPECTED_RETURN(bytes);

        IpAddr addr = IpAddr::from_bytes(*family, *bytes);
        if (addr.has_ip())
        {
            servers.push_back({
                .addr = std::move(addr),
                .interface_name = interfaces::name_of(static_cast<uint32_t>(*index)).value_or(""),
            });
        }
    }
    return servers;
}

} // namespace

Result<std::vector<DnsServer>> servers(Duration resolver_timeout)
{
    if constexpr (build::is_android)
        return std::unexpected(Error(ErrorCode::not_supported, "android resolves through netd, without resolv.conf"));

    auto content = sihd::sys::fs::read_all(resolv_conf_path);
    SIHD_UNEXPECTED_RETURN(content);
    std::vector<DnsServer> resolv_conf_servers = parse_resolv_conf(*content);

    if (dbus::Connection::supported && !resolv_conf_servers.empty()
        && std::ranges::all_of(resolv_conf_servers, is_resolved_stub))
    {
        if (auto resolved = resolved_servers(resolver_timeout))
            return resolved;
    }
    return resolv_conf_servers;
}

} // namespace sihd::net::dns

#endif
