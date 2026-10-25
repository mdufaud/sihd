#include "wlanapi.hpp"

#include <algorithm>
#include <array>
#include <span>

#include <sihd/sys/os.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/str.hpp>

using namespace sihd::util;
using enum ErrorCode;

namespace sihd::wifi::wlanapi
{

namespace
{

constexpr DWORD client_version = 2;

template <typename Function>
Result<void> load_symbol(const sihd::sys::DynLib & library, ZStringView name, Function *& function)
{
    auto symbol = library.load_function<Function>(name);
    SIHD_UNEXPECTED_RETURN(symbol);
    function = *symbol;
    return {};
}

Result<void> load(Api & api)
{
    auto opened = api.library.open_system("wlanapi");
    if (!opened && opened.error().code == not_found)
        return std::unexpected(Error(not_supported, "wlanapi unavailable: {}", opened.error().message));
    SIHD_UNEXPECTED_RETURN_CTX(opened, "loading wlanapi");

    const std::array<Result<void>, 15> loaded = {
        load_symbol(api.library, "WlanOpenHandle", api.open_handle),
        load_symbol(api.library, "WlanCloseHandle", api.close_handle),
        load_symbol(api.library, "WlanEnumInterfaces", api.enum_interfaces),
        load_symbol(api.library, "WlanQueryInterface", api.query_interface),
        load_symbol(api.library, "WlanGetNetworkBssList", api.get_network_bss_list),
        load_symbol(api.library, "WlanGetAvailableNetworkList", api.get_available_network_list),
        load_symbol(api.library, "WlanScan", api.scan),
        load_symbol(api.library, "WlanConnect", api.connect),
        load_symbol(api.library, "WlanDisconnect", api.disconnect),
        load_symbol(api.library, "WlanSetProfile", api.set_profile),
        load_symbol(api.library, "WlanGetProfile", api.get_profile),
        load_symbol(api.library, "WlanGetProfileList", api.get_profile_list),
        load_symbol(api.library, "WlanDeleteProfile", api.delete_profile),
        load_symbol(api.library, "WlanRegisterNotification", api.register_notification),
        load_symbol(api.library, "WlanFreeMemory", api.free_memory),
    };
    for (const Result<void> & symbol : loaded)
        SIHD_UNEXPECTED_RETURN(symbol);
    return {};
}

bool is_awaited(const Notification & notification, const GUID & guid, const std::vector<DWORD> & codes)
{
    return notification.interface_guid == guid && std::ranges::find(codes, notification.code) != codes.end();
}

DWORD reason_of(const WLAN_NOTIFICATION_DATA & data)
{
    if (data.NotificationCode == wlan_notification_acm_scan_fail && data.dwDataSize >= sizeof(WLAN_REASON_CODE))
        return *static_cast<const WLAN_REASON_CODE *>(data.pData);
    const bool connection = data.NotificationCode == wlan_notification_acm_connection_complete
                            || data.NotificationCode == wlan_notification_acm_connection_attempt_fail;
    if (connection && data.dwDataSize >= sizeof(WLAN_CONNECTION_NOTIFICATION_DATA))
        return static_cast<const WLAN_CONNECTION_NOTIFICATION_DATA *>(data.pData)->wlanReasonCode;
    return 0;
}

} // namespace

Session::~Session()
{
    // the handle must close before the queue goes away: closing stops the notification callbacks
    if (_client != nullptr)
        (void)_api.close_handle(_client, nullptr);
}

Result<GUID> Session::open(const std::string & interface_name)
{
    if (_client != nullptr)
        (void)_api.close_handle(_client, nullptr);
    _client = nullptr;
    auto loaded = load(_api);
    SIHD_UNEXPECTED_RETURN(loaded);
    DWORD negotiated_version = 0;
    if (const DWORD code = _api.open_handle(client_version, nullptr, &negotiated_version, &_client);
        code != ERROR_SUCCESS)
        return std::unexpected(error(code, "wlan session"));
    return this->_find_interface(interface_name);
}

Result<GUID> Session::_find_interface(const std::string & name)
{
    const Error missing(not_found, "no wifi interface named {}", name);
    NET_LUID luid {};
    if (ConvertInterfaceAliasToLuid(wide(name).c_str(), &luid) != NO_ERROR)
        return std::unexpected(missing);
    GUID guid {};
    if (const DWORD code = ConvertInterfaceLuidToGuid(&luid, &guid); code != NO_ERROR)
        return std::unexpected(error(code, "interface guid"));

    PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
    if (const DWORD code = _api.enum_interfaces(_client, nullptr, &interfaces); code != ERROR_SUCCESS)
        return std::unexpected(error(code, "wlan interfaces"));
    Defer free_interfaces([this, interfaces] { _api.free_memory(interfaces); });

    const std::span<const WLAN_INTERFACE_INFO> listed(interfaces->InterfaceInfo, interfaces->dwNumberOfItems);
    if (std::ranges::none_of(listed, [&guid](const WLAN_INTERFACE_INFO & info) { return info.InterfaceGuid == guid; }))
        return std::unexpected(missing);
    return guid;
}

Result<std::optional<WLAN_CONNECTION_ATTRIBUTES>> Session::connection(const GUID & guid)
{
    DWORD data_size = 0;
    void *data = nullptr;
    const DWORD code = _api.query_interface(_client,
                                            &guid,
                                            wlan_intf_opcode_current_connection,
                                            nullptr,
                                            &data_size,
                                            &data,
                                            nullptr);
    if (code == ERROR_INVALID_STATE)
        return std::nullopt;
    if (code != ERROR_SUCCESS)
        return std::unexpected(error(code, "wlan connection"));
    Defer free_connection([this, data] { _api.free_memory(data); });

    const auto *attributes = static_cast<const WLAN_CONNECTION_ATTRIBUTES *>(data);
    if (attributes->isState != wlan_interface_state_connected)
        return std::nullopt;
    return *attributes;
}

Result<WLAN_INTERFACE_STATE> Session::interface_state(const GUID & guid)
{
    DWORD data_size = 0;
    void *data = nullptr;
    if (const DWORD code = _api.query_interface(_client,
                                                &guid,
                                                wlan_intf_opcode_interface_state,
                                                nullptr,
                                                &data_size,
                                                &data,
                                                nullptr);
        code != ERROR_SUCCESS)
        return std::unexpected(error(code, "wlan interface state"));
    Defer free_state([this, data] { _api.free_memory(data); });
    return *static_cast<const WLAN_INTERFACE_STATE *>(data);
}

Result<void> Session::listen()
{
    if (const DWORD code = _api.register_notification(_client,
                                                      WLAN_NOTIFICATION_SOURCE_ACM,
                                                      TRUE,
                                                      &Session::_on_notification,
                                                      this,
                                                      nullptr,
                                                      nullptr);
        code != ERROR_SUCCESS)
        return std::unexpected(error(code, "wlan notifications"));
    return {};
}

std::optional<Notification> Session::wait(const GUID & guid, const std::vector<DWORD> & codes, Duration timeout)
{
    auto guarded = _waitable.wait_for_guard(timeout, [this, &guid, &codes] {
        return std::ranges::find_if(
                   _notifications,
                   [&guid, &codes](const Notification & notification) { return is_awaited(notification, guid, codes); })
               != _notifications.end();
    });
    const auto found = std::ranges::find_if(_notifications, [&guid, &codes](const Notification & notification) {
        return is_awaited(notification, guid, codes);
    });
    if (guarded.timed_out || found == _notifications.end())
        return std::nullopt;
    const Notification notification = *found;
    _notifications.erase(found);
    return notification;
}

std::optional<Notification> Session::next(Duration timeout)
{
    auto guarded = _waitable.wait_for_guard(timeout, [this] { return !_notifications.empty(); });
    if (guarded.timed_out || _notifications.empty())
        return std::nullopt;
    const Notification notification = _notifications.front();
    _notifications.pop_front();
    return notification;
}

const Api & Session::api() const
{
    return _api;
}

HANDLE Session::client() const
{
    return _client;
}

void WINAPI Session::_on_notification(PWLAN_NOTIFICATION_DATA data, PVOID context)
{
    if (data == nullptr || data->NotificationSource != WLAN_NOTIFICATION_SOURCE_ACM)
        return;
    Session *session = static_cast<Session *>(context);
    {
        auto lock = session->_waitable.guard();
        session->_notifications.push_back(Notification {
            .interface_guid = data->InterfaceGuid,
            .code = data->NotificationCode,
            .reason = reason_of(*data),
        });
    }
    session->_waitable.notify_all();
}

Error error(unsigned long code, const std::string & context)
{
    ErrorCode error_code = io_error;
    if (code == ERROR_ACCESS_DENIED)
        error_code = permission_denied;
    else if (code == ERROR_INVALID_STATE)
        error_code = not_found;
    else if (code == ERROR_ALREADY_EXISTS)
        error_code = already_exists;
    else if (code == ERROR_SERVICE_NOT_ACTIVE || code == ERROR_CALL_NOT_IMPLEMENTED || code == ERROR_NOT_SUPPORTED)
        error_code = not_supported;
    return Error(error_code, "{}: {}", context, sihd::sys::os::error_str(static_cast<int>(code)));
}

std::wstring wide(const std::string & text)
{
    return str::to_wstr(text);
}

std::string narrow(const WCHAR *text)
{
    return str::to_str(std::wstring_view(text));
}

} // namespace sihd::wifi::wlanapi
