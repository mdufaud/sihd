#ifndef __SIHD_WIFI_CONNECT_HPP__
#define __SIHD_WIFI_CONNECT_HPP__

#include <optional>
#include <string>

#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/wifi/scan.hpp>

namespace sihd::wifi
{

struct Target
{
        Security security = Security::unknown;
        bool known = false;
};

// every backend must refuse the same connect with the same error
sihd::util::Result<void>
    check_target(const std::string & ssid, const Target & target, const std::optional<std::string> & passphrase);

sihd::util::Result<void> connect_backend(const std::string & interface_name,
                                         const std::string & ssid,
                                         const std::optional<std::string> & passphrase,
                                         sihd::util::Duration timeout);

} // namespace sihd::wifi

#endif
