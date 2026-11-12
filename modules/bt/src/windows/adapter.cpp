#include <winsock2.h>

#include <bluetoothapis.h>

#include <sihd/bt/adapter.hpp>
#include <sihd/util/time.hpp>

#include "../address.hpp"
#include "winrt.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;
using winrt::ComPtr;
namespace bluetooth = ABI::Windows::Devices::Bluetooth;
namespace enumeration = ABI::Windows::Devices::Enumeration;
namespace radios = ABI::Windows::Devices::Radios;
namespace collections = ABI::Windows::Foundation::Collections;

namespace
{

const Duration call_timeout(time::sec(10));

struct Adapter
{
        std::string identifier;
        std::string name;
        UINT64 address = 0;
        ComPtr<bluetooth::IBluetoothAdapter> adapter;
};

Result<Adapter> adapter_of(bluetooth::IBluetoothAdapterStatics *statics, enumeration::IDeviceInformation *information)
{
    Microsoft::WRL::Wrappers::HString id;
    Microsoft::WRL::Wrappers::HString name;
    ComPtr<winrt::foundation::IAsyncOperation<bluetooth::BluetoothAdapter *>> opening;
    HRESULT hr = information->get_Id(id.GetAddressOf());
    if (SUCCEEDED(hr))
        hr = information->get_Name(name.GetAddressOf());
    if (SUCCEEDED(hr))
        hr = statics->FromIdAsync(id.Get(), &opening);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth adapter"));
    auto opened = winrt::await(opening, call_timeout, "bluetooth adapter");
    SIHD_UNEXPECTED_RETURN(opened);

    Adapter adapter {.identifier = winrt::to_utf8(id.Get()),
                     .name = winrt::to_utf8(name.Get()),
                     .address = 0,
                     .adapter = *opened};
    hr = adapter.adapter->get_BluetoothAddress(&adapter.address);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth adapter address"));
    return adapter;
}

// the adapters windows lists, in its order
Result<std::vector<Adapter>> adapters()
{
    auto statics = winrt::factory<bluetooth::IBluetoothAdapterStatics>(
        RuntimeClass_Windows_Devices_Bluetooth_BluetoothAdapter);
    SIHD_UNEXPECTED_RETURN(statics);
    auto informations = winrt::factory<enumeration::IDeviceInformationStatics>(
        RuntimeClass_Windows_Devices_Enumeration_DeviceInformation);
    SIHD_UNEXPECTED_RETURN(informations);

    Microsoft::WRL::Wrappers::HString selector;
    ComPtr<winrt::foundation::IAsyncOperation<enumeration::DeviceInformationCollection *>> finding;
    HRESULT hr = (*statics)->GetDeviceSelector(selector.GetAddressOf());
    if (SUCCEEDED(hr))
        hr = (*informations)->FindAllAsyncAqsFilter(selector.Get(), &finding);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth adapters"));
    auto found = winrt::await(finding, call_timeout, "bluetooth adapters");
    SIHD_UNEXPECTED_RETURN(found);

    UINT32 count = 0;
    (*found)->get_Size(&count);
    std::vector<Adapter> listed;
    for (UINT32 i = 0; i < count; ++i)
    {
        ComPtr<enumeration::IDeviceInformation> information;
        if (FAILED((*found)->GetAt(i, &information)))
            continue;
        auto adapter = adapter_of(statics->Get(), information.Get());
        SIHD_UNEXPECTED_RETURN(adapter);
        listed.emplace_back(std::move(*adapter));
    }
    return listed;
}

Result<Adapter> adapter_at(size_t index)
{
    auto listed = adapters();
    SIHD_UNEXPECTED_RETURN(listed);
    if (index >= listed->size())
        return std::unexpected(Error(not_found, "no bluetooth adapter at index {}", index));
    return std::move(listed->at(index));
}

Result<ComPtr<radios::IRadio>> radio_of(bluetooth::IBluetoothAdapter *adapter)
{
    ComPtr<winrt::foundation::IAsyncOperation<radios::Radio *>> getting;
    const HRESULT hr = adapter->GetRadioAsync(&getting);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth radio"));
    return winrt::await(getting, call_timeout, "bluetooth radio");
}

Result<bool> powered(bluetooth::IBluetoothAdapter *adapter)
{
    auto radio = radio_of(adapter);
    SIHD_UNEXPECTED_RETURN(radio);
    radios::RadioState state = radios::RadioState_Unknown;
    // mingw-w64 declares a RadioState ** where windows writes a RadioState
    const HRESULT hr = (*radio)->get_State(reinterpret_cast<radios::RadioState **>(&state));
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth radio state"));
    return state == radios::RadioState_On;
}

// visits the Win32 handle of each radio until visit returns true
void each_radio(const std::function<bool(HANDLE, const BLUETOOTH_RADIO_INFO &)> & visit)
{
    BLUETOOTH_FIND_RADIO_PARAMS params {};
    params.dwSize = sizeof(params);
    HANDLE radio = nullptr;
    HBLUETOOTH_RADIO_FIND find = BluetoothFindFirstRadio(&params, &radio);
    for (bool found = find != nullptr; found; found = BluetoothFindNextRadio(find, &radio) != FALSE)
    {
        BLUETOOTH_RADIO_INFO info {};
        info.dwSize = sizeof(info);
        const bool done = BluetoothGetRadioInfo(radio, &info) == ERROR_SUCCESS && visit(radio, info);
        CloseHandle(radio);
        if (done)
            break;
    }
    if (find != nullptr)
        BluetoothFindRadioClose(find);
}

bool discoverable(UINT64 address)
{
    bool visible = false;
    each_radio([&](HANDLE radio, const BLUETOOTH_RADIO_INFO & info) {
        if (info.address.ullLong != address)
            return false;
        visible = BluetoothIsDiscoverable(radio) != FALSE;
        return true;
    });
    return visible;
}

} // namespace

Result<bool> bluetooth_enabled()
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto listed = adapters();
    SIHD_UNEXPECTED_RETURN(listed);
    for (const Adapter & adapter : *listed)
    {
        auto on = powered(adapter.adapter.Get());
        if (on.value_or(false))
            return true;
    }
    return false;
}

Result<std::vector<AdapterInfo>> list_adapters()
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto listed = adapters();
    if (!listed && listed.error().code == not_supported)
        return std::vector<AdapterInfo> {};
    SIHD_UNEXPECTED_RETURN(listed);

    std::vector<AdapterInfo> infos;
    for (const Adapter & adapter : *listed)
    {
        const bool on = powered(adapter.adapter.Get()).value_or(false);
        infos.push_back(AdapterInfo {
            .identifier = adapter.identifier,
            .address = format_address(adapter.address),
            .name = adapter.name,
            .alias = adapter.name,
            .powered = on,
            .discoverable = on && discoverable(adapter.address),
            .pairable = on,
            .discovering = false,
        });
    }
    return infos;
}

Result<void> set_adapter_powered(size_t index, bool powered)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto adapter = adapter_at(index);
    SIHD_UNEXPECTED_RETURN(adapter);
    auto radio = radio_of(adapter->adapter.Get());
    SIHD_UNEXPECTED_RETURN(radio);

    ComPtr<winrt::foundation::IAsyncOperation<radios::RadioAccessStatus>> setting;
    const HRESULT hr = (*radio)->SetStateAsync(powered ? radios::RadioState_On : radios::RadioState_Off, &setting);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth power"));
    auto access = winrt::await(setting, call_timeout, "bluetooth power");
    SIHD_UNEXPECTED_RETURN(access);
    if (*access != radios::RadioAccessStatus_Allowed)
        return std::unexpected(Error(permission_denied, "bluetooth power: refused by the user or the system"));
    return {};
}

Result<void> set_adapter_discoverable(size_t index, bool discoverable)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto adapter = adapter_at(index);
    SIHD_UNEXPECTED_RETURN(adapter);

    std::optional<bool> set;
    each_radio([&](HANDLE radio, const BLUETOOTH_RADIO_INFO & info) {
        if (info.address.ullLong != adapter->address)
            return false;
        // only a connectable radio is discoverable
        set = (!discoverable || BluetoothEnableIncomingConnections(radio, TRUE) != FALSE)
              && BluetoothEnableDiscovery(radio, discoverable ? TRUE : FALSE) != FALSE;
        return true;
    });
    if (!set.has_value())
        return std::unexpected(Error(not_found, "bluetooth radio of {} is off", format_address(adapter->address)));
    if (!*set)
        return std::unexpected(Error(io_error, "the bluetooth radio refused to change its discoverability"));
    return {};
}

Result<void> set_adapter_pairable(size_t index, bool)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto adapter = adapter_at(index);
    SIHD_UNEXPECTED_RETURN(adapter);
    return std::unexpected(Error(not_supported, "windows accepts pairings while bluetooth is on"));
}

Result<void> set_adapter_alias(size_t index, const std::string &)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto adapter = adapter_at(index);
    SIHD_UNEXPECTED_RETURN(adapter);
    return std::unexpected(Error(not_supported, "windows names the adapter after the computer"));
}

} // namespace sihd::bt
