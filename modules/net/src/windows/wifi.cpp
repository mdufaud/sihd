#include <winsock2.h>

#include <wlanapi.h>

#include <algorithm>
#include <cstring>
#include <span>
#include <vector>

#include <sihd/net/ip.hpp>
#include <sihd/net/wifi.hpp>
#include <sihd/sys/DynLib.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Error.hpp>

#include "adapters.hpp"

using namespace sihd::util;
using enum ErrorCode;

namespace sihd::net::wifi
{

namespace
{

constexpr unsigned long adapter_flags = GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                                        | GAA_FLAG_SKIP_DNS_SERVER;

// wlanSignalQuality 0..100 stands for -100..-50 dBm, linear in between
constexpr int quality_floor_dbm = -100;
constexpr int quality_points_per_dbm = 2;

// loaded at runtime: windows server ships without wlanapi.dll until its wireless feature is installed
struct WlanApi
{
        sihd::sys::DynLib library;
        decltype(&WlanOpenHandle) open_handle = nullptr;
        decltype(&WlanCloseHandle) close_handle = nullptr;
        decltype(&WlanEnumInterfaces) enum_interfaces = nullptr;
        decltype(&WlanQueryInterface) query_interface = nullptr;
        decltype(&WlanGetNetworkBssList) get_network_bss_list = nullptr;
        decltype(&WlanFreeMemory) free_memory = nullptr;
};

template <typename Function>
Result<void> load_symbol(const sihd::sys::DynLib & library, const std::string & name, Function *& function)
{
    auto symbol = library.load_function<Function>(name);
    SIHD_UNEXPECTED_RETURN(symbol);
    function = *symbol;
    return {};
}

Result<void> load_wlanapi(WlanApi & api)
{
    auto opened = api.library.open_system("wlanapi");
    if (!opened && opened.error().code == not_found)
        return std::unexpected(Error(not_supported, "wlanapi unavailable: {}", opened.error().message));
    SIHD_UNEXPECTED_RETURN_CTX(opened, "loading wlanapi");

    auto open_handle = load_symbol(api.library, "WlanOpenHandle", api.open_handle);
    SIHD_UNEXPECTED_RETURN(open_handle);
    auto close_handle = load_symbol(api.library, "WlanCloseHandle", api.close_handle);
    SIHD_UNEXPECTED_RETURN(close_handle);
    auto enum_interfaces = load_symbol(api.library, "WlanEnumInterfaces", api.enum_interfaces);
    SIHD_UNEXPECTED_RETURN(enum_interfaces);
    auto query_interface = load_symbol(api.library, "WlanQueryInterface", api.query_interface);
    SIHD_UNEXPECTED_RETURN(query_interface);
    auto get_network_bss_list = load_symbol(api.library, "WlanGetNetworkBssList", api.get_network_bss_list);
    SIHD_UNEXPECTED_RETURN(get_network_bss_list);
    auto free_memory = load_symbol(api.library, "WlanFreeMemory", api.free_memory);
    SIHD_UNEXPECTED_RETURN(free_memory);
    return {};
}

// location consent gates the connection details since windows 11 24H2
Error wlan_error(unsigned long code, const std::string & context)
{
    ErrorCode error_code = io_error;
    if (code == ERROR_ACCESS_DENIED)
        error_code = permission_denied;
    else if (code == ERROR_INVALID_STATE)
        error_code = not_found;
    else if (code == ERROR_SERVICE_NOT_ACTIVE || code == ERROR_CALL_NOT_IMPLEMENTED || code == ERROR_NOT_SUPPORTED)
        error_code = not_supported;
    return Error(error_code, "{}: {}", context, sihd::sys::os::error_str(static_cast<int>(code)));
}

const IP_ADAPTER_ADDRESSES *find_adapter(const adapters::AdapterList & list, const std::string & interface_name)
{
    const std::vector<const IP_ADAPTER_ADDRESSES *> named = adapters::named(list);
    const auto found = std::ranges::find_if(named, [&interface_name](const IP_ADAPTER_ADDRESSES *adapter) {
        return adapters::friendly_name(*adapter) == interface_name;
    });
    return found == named.end() ? nullptr : *found;
}

bool is_wlan_interface(const WLAN_INTERFACE_INFO_LIST & interfaces, const GUID & guid)
{
    return std::ranges::any_of(std::span(interfaces.InterfaceInfo, interfaces.dwNumberOfItems),
                               [&guid](const WLAN_INTERFACE_INFO & info) { return info.InterfaceGuid == guid; });
}

void read_bss(const WlanApi & api,
              HANDLE client,
              const GUID & guid,
              const WLAN_CONNECTION_ATTRIBUTES & connection,
              Link & result)
{
    DOT11_SSID ssid = connection.wlanAssociationAttributes.dot11Ssid;
    PWLAN_BSS_LIST bss_list = nullptr;
    if (api.get_network_bss_list(client,
                                 &guid,
                                 &ssid,
                                 dot11_BSS_type_infrastructure,
                                 connection.wlanSecurityAttributes.bSecurityEnabled,
                                 nullptr,
                                 &bss_list)
        != ERROR_SUCCESS)
        return;
    Defer free_bss_list([&api, bss_list] { api.free_memory(bss_list); });

    const std::span<const WLAN_BSS_ENTRY> entries(bss_list->wlanBssEntries, bss_list->dwNumberOfItems);
    const auto entry = std::ranges::find_if(entries, [&connection](const WLAN_BSS_ENTRY & candidate) {
        return std::memcmp(candidate.dot11Bssid,
                           connection.wlanAssociationAttributes.dot11Bssid,
                           sizeof(DOT11_MAC_ADDRESS))
               == 0;
    });
    if (entry == entries.end())
        return;
    result.signal_dbm = static_cast<int>(entry->lRssi);
    result.frequency_mhz = static_cast<uint32_t>(entry->ulChCenterFrequency / 1000);
}

} // namespace

Result<Link> link(const std::string & interface_name)
{
    auto list = adapters::list(adapter_flags);
    SIHD_UNEXPECTED_RETURN(list);
    const IP_ADAPTER_ADDRESSES *adapter = find_adapter(*list, interface_name);
    if (adapter == nullptr)
        return std::unexpected(Error(not_found, "no interface named {}", interface_name));
    GUID guid {};
    if (const unsigned long code = ConvertInterfaceLuidToGuid(&adapter->Luid, &guid); code != NO_ERROR)
        return std::unexpected(wlan_error(code, "interface guid"));

    WlanApi api;
    auto loaded = load_wlanapi(api);
    SIHD_UNEXPECTED_RETURN(loaded);

    HANDLE client = nullptr;
    DWORD negotiated_version = 0;
    if (const unsigned long code = api.open_handle(2, nullptr, &negotiated_version, &client); code != ERROR_SUCCESS)
        return std::unexpected(wlan_error(code, "wlan session"));
    Defer close_client([&api, client] { api.close_handle(client, nullptr); });

    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
    if (const unsigned long code = api.enum_interfaces(client, nullptr, &interfaces); code != ERROR_SUCCESS)
        return std::unexpected(wlan_error(code, "wlan interfaces"));
    Defer free_interfaces([&api, interfaces] { api.free_memory(interfaces); });
    if (!is_wlan_interface(*interfaces, guid))
        return std::unexpected(Error(not_found, "{} is not a wifi interface", interface_name));

    DWORD data_size = 0;
    void *data = nullptr;
    if (const unsigned long code = api.query_interface(client,
                                                       &guid,
                                                       wlan_intf_opcode_current_connection,
                                                       nullptr,
                                                       &data_size,
                                                       &data,
                                                       nullptr);
        code != ERROR_SUCCESS)
        return std::unexpected(wlan_error(code, fmt::format("{} connection", interface_name)));
    Defer free_connection([&api, data] { api.free_memory(data); });

    const auto *connection = static_cast<const WLAN_CONNECTION_ATTRIBUTES *>(data);
    if (connection->isState != wlan_interface_state_connected)
        return std::unexpected(Error(not_found, "{} is not associated", interface_name));

    const WLAN_ASSOCIATION_ATTRIBUTES & association = connection->wlanAssociationAttributes;
    Link result;
    result.ssid.assign(association.dot11Ssid.ucSSID, association.dot11Ssid.ucSSID + association.dot11Ssid.uSSIDLength);
    result.bssid = ip::mac_str(association.dot11Bssid);
    result.tx_bitrate_bps = static_cast<uint64_t>(association.ulTxRate) * 1000;
    result.signal_dbm = quality_floor_dbm + static_cast<int>(association.wlanSignalQuality) / quality_points_per_dbm;
    read_bss(api, client, guid, *connection, result);
    return result;
}

} // namespace sihd::net::wifi
