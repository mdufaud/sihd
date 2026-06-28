#include <simpleble/Adapter.h>

#include <sihd/bt/BtUtils.hpp>
#include <sihd/util/Logger.hpp>

namespace sihd::bt
{

SIHD_NEW_LOGGER("sihd::bt");

BtUtils::BtUtils() = default;

BtUtils::~BtUtils() = default;

bool BtUtils::bluetooth_enabled()
{
    return SimpleBLE::Adapter::bluetooth_enabled();
}

std::vector<BtDevice> BtUtils::scan(int timeout_ms)
{
    std::vector<BtDevice> devices;
    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty())
    {
        SIHD_LOG(warning, "BtUtils: no bluetooth adapter found");
        return devices;
    }
    auto & adapter = adapters[0];
    SIHD_LOG(info, "BtUtils: scanning with adapter {} [{}]", adapter.identifier(), adapter.address());
    adapter.scan_for(timeout_ms);
    for (auto & peripheral : adapter.scan_get_results())
    {
        devices.push_back({
            .identifier = peripheral.identifier(),
            .address = peripheral.address(),
            .rssi = peripheral.rssi(),
            .connectable = peripheral.is_connectable(),
        });
    }
    SIHD_LOG(info, "BtUtils: found {} devices", devices.size());
    return devices;
}

} // namespace sihd::bt