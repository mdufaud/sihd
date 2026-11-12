#ifndef __SIHD_BT_ADAPTER_HPP__
#define __SIHD_BT_ADAPTER_HPP__

#include <cstddef>
#include <string>
#include <vector>

#include <sihd/util/Result.hpp>

namespace sihd::bt
{

enum class AddressType
{
    public_address,
    random,
    unspecified
};

struct AdapterInfo
{
        std::string identifier;
        std::string address;
        std::string name;
        std::string alias;
        bool powered = false;
        bool discoverable = false;
        bool pairable = false;
        bool discovering = false;
};

sihd::util::Result<bool> bluetooth_enabled();
sihd::util::Result<std::vector<AdapterInfo>> list_adapters();

sihd::util::Result<void> set_adapter_powered(size_t index, bool powered);
sihd::util::Result<void> set_adapter_discoverable(size_t index, bool discoverable);
sihd::util::Result<void> set_adapter_pairable(size_t index, bool pairable);
sihd::util::Result<void> set_adapter_alias(size_t index, const std::string & alias);

} // namespace sihd::bt

#endif
