#include <algorithm>

#include <sihd/bt/adapter.hpp>

#include "bluez.hpp"

namespace sihd::bt
{

using namespace sihd::util;

namespace
{

template <typename T>
Result<void> set_adapter_property(size_t index, ZStringView name, const T & value)
{
    auto connection = bluez::open_system_bus();
    SIHD_UNEXPECTED_RETURN(connection);
    auto path = bluez::adapter_path(*connection, index);
    SIHD_UNEXPECTED_RETURN(path);
    return bluez::set_property(*connection, *path, bluez::adapter_interface, name, value);
}

} // namespace

Result<bool> bluetooth_enabled()
{
    auto objects = bluez::open_managed_objects();
    SIHD_UNEXPECTED_RETURN(objects);
    return std::ranges::any_of(objects->adapters,
                               [](const bluez::AdapterObject & adapter) { return adapter.info.powered; });
}

Result<std::vector<AdapterInfo>> list_adapters()
{
    auto objects = bluez::open_managed_objects();
    SIHD_UNEXPECTED_RETURN(objects);

    std::vector<AdapterInfo> adapters;
    for (const bluez::AdapterObject & adapter : objects->adapters)
        adapters.emplace_back(adapter.info);
    return adapters;
}

Result<void> set_adapter_powered(size_t index, bool powered)
{
    return set_adapter_property(index, "Powered", powered);
}

Result<void> set_adapter_discoverable(size_t index, bool discoverable)
{
    return set_adapter_property(index, "Discoverable", discoverable);
}

Result<void> set_adapter_pairable(size_t index, bool pairable)
{
    return set_adapter_property(index, "Pairable", pairable);
}

Result<void> set_adapter_alias(size_t index, const std::string & alias)
{
    return set_adapter_property(index, "Alias", alias);
}

} // namespace sihd::bt
