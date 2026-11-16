#ifndef __SIHD_NET_LOGGERENDPOINT_HPP__
#define __SIHD_NET_LOGGERENDPOINT_HPP__

#include <string>
#include <string_view>

#include <sihd/net/IpAddr.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::net
{

// an ip literal and a port, or a unix socket path, never both
class LoggerEndpoint
{
    public:
        LoggerEndpoint();
        ~LoggerEndpoint() = default;

        sihd::util::Result<void> set_host(std::string_view host);
        sihd::util::Result<void> set_port(int port);
        sihd::util::Result<void> set_unix_path(std::string_view path);

        const std::string & host() const;
        int port() const;
        const std::string & unix_path() const;

        IpAddr ip_addr() const;
        std::string str() const;

    private:
        std::string _host;
        int _port;
        std::string _unix_path;
};

} // namespace sihd::net

#endif
