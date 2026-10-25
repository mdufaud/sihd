#ifndef __SIHD_WIFI_WINDOWS_WLANAPI_HPP__
#define __SIHD_WIFI_WINDOWS_WLANAPI_HPP__

// clang-format off
// netioapi.h, pulled by iphlpapi.h, declares nothing unless ws2tcpip.h came first
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <wlanapi.h>
// clang-format on

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include <sihd/sys/DynLib.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Waitable.hpp>

namespace sihd::wifi::wlanapi
{

constexpr unsigned long khz_per_mhz = 1000;

// loaded at runtime: windows server never ships wlanapi.dll until its wireless feature is installed
struct Api
{
        sihd::sys::DynLib library;
        decltype(&WlanOpenHandle) open_handle = nullptr;
        decltype(&WlanCloseHandle) close_handle = nullptr;
        decltype(&WlanEnumInterfaces) enum_interfaces = nullptr;
        decltype(&WlanQueryInterface) query_interface = nullptr;
        decltype(&WlanGetNetworkBssList) get_network_bss_list = nullptr;
        decltype(&WlanGetAvailableNetworkList) get_available_network_list = nullptr;
        decltype(&WlanScan) scan = nullptr;
        decltype(&WlanConnect) connect = nullptr;
        decltype(&WlanDisconnect) disconnect = nullptr;
        decltype(&WlanSetProfile) set_profile = nullptr;
        decltype(&WlanGetProfile) get_profile = nullptr;
        decltype(&WlanGetProfileList) get_profile_list = nullptr;
        decltype(&WlanDeleteProfile) delete_profile = nullptr;
        decltype(&WlanRegisterNotification) register_notification = nullptr;
        decltype(&WlanFreeMemory) free_memory = nullptr;
};

struct Notification
{
        GUID interface_guid {};
        DWORD code = 0;
        // WLAN_REASON_CODE of a failure, 0 otherwise
        DWORD reason = 0;
};

class Session
{
    public:
        Session() = default;
        ~Session();

        Session(const Session &) = delete;
        Session & operator=(const Session &) = delete;

        // not_supported unless wlanapi.dll exists, not_found unless a wifi interface of that name
        sihd::util::Result<GUID> open(const std::string & interface_name);
        // nullopt when the interface is not associated
        sihd::util::Result<std::optional<WLAN_CONNECTION_ATTRIBUTES>> connection(const GUID & guid);
        sihd::util::Result<WLAN_INTERFACE_STATE> interface_state(const GUID & guid);

        // must come before acting on the interface: queues the acm notifications from here on
        sihd::util::Result<void> listen();
        // the first queued notification of that interface among the codes, nullopt once the timeout elapsed
        std::optional<Notification>
            wait(const GUID & guid, const std::vector<DWORD> & codes, sihd::util::Duration timeout);
        // nullopt once the timeout elapsed
        std::optional<Notification> next(sihd::util::Duration timeout);

        const Api & api() const;
        HANDLE client() const;

    private:
        static void WINAPI _on_notification(PWLAN_NOTIFICATION_DATA data, PVOID context);

        sihd::util::Result<GUID> _find_interface(const std::string & name);

        Api _api;
        HANDLE _client = nullptr;
        sihd::util::Waitable _waitable;
        std::deque<Notification> _notifications;
};

// the wlan details need the location consent since windows 11 24H2, access is denied otherwise
sihd::util::Error error(unsigned long code, const std::string & context);

std::wstring wide(const std::string & text);
std::string narrow(const WCHAR *text);

} // namespace sihd::wifi::wlanapi

#endif
