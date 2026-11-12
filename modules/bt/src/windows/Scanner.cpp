#include <winsock2.h>

#include <bluetoothapis.h>

#include <algorithm>
#include <thread>

#include <sihd/bt/Scanner.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#include "../address.hpp"
#include "winrt.hpp"

namespace sihd::bt
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using namespace ABI::Windows::Devices::Bluetooth::Advertisement;
using winrt::ComPtr;

SIHD_LOGGER;

namespace
{

using ReceivedEventHandler = ABI::Windows::Foundation::ITypedEventHandler<BluetoothLEAdvertisementWatcher *,
                                                                          BluetoothLEAdvertisementReceivedEventArgs *>;

const Duration max_wait(time::milli(500));
// classic inquiries of 2 x 1.28 seconds: a stop waits for the round in flight
constexpr UCHAR inquiry_multiplier = 2;

// pushes the received advertisements to the scan loop; a dispatch in flight holds a reference
class ReceivedHandler: public ReceivedEventHandler
{
    public:
        ReceivedHandler(Waitable & waitable, std::vector<ScanResult> & pending):
            _waitable(waitable),
            _pending(pending),
            _references(1)
        {
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
        {
            if (riid == IID_IUnknown || riid == IID_IAgileObject || riid == __uuidof(ReceivedEventHandler))
            {
                *object = this;
                this->AddRef();
                return S_OK;
            }
            *object = nullptr;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override { return ++_references; }

        ULONG STDMETHODCALLTYPE Release() override
        {
            // the scan may destroy this handler as soon as the count drops: no member access after it
            Waitable & waitable = _waitable;
            ULONG references = 0;
            {
                auto lock = waitable.guard();
                references = --_references;
            }
            waitable.notify_all();
            return references;
        }

        HRESULT STDMETHODCALLTYPE Invoke(IBluetoothLEAdvertisementWatcher *watcher,
                                         IBluetoothLEAdvertisementReceivedEventArgs *args) override;

        // the scan owns one reference: the others belong to the watcher or to a dispatch in flight
        bool released_by_others() const { return _references.load() == 1; }

    private:
        Waitable & _waitable;
        // guarded by _waitable
        std::vector<ScanResult> & _pending;
        std::atomic<ULONG> _references;
};

// little endian 16, 32 or 128 bits uuid in the bluez text form
std::string service_uuid(const std::vector<uint8_t> & bytes, size_t uuid_size)
{
    std::string hex;
    for (size_t i = uuid_size; i > 0; --i)
        hex += fmt::format("{:02x}", bytes[i - 1]);
    if (uuid_size < 16)
        return fmt::format("{:0>8}-0000-1000-8000-00805f9b34fb", hex);
    return fmt::format("{}-{}-{}-{}-{}",
                       hex.substr(0, 8),
                       hex.substr(8, 4),
                       hex.substr(12, 4),
                       hex.substr(16, 4),
                       hex.substr(20));
}

// advertisement data types 0x16, 0x20 and 0x21: a 16, 32 or 128 bits service uuid then its data
void read_service_data(IBluetoothLEAdvertisement *advertisement, ScanResult & result)
{
    ComPtr<ABI::Windows::Foundation::Collections::IVector<BluetoothLEAdvertisementDataSection *>> sections;
    UINT32 section_count = 0;
    if (FAILED(advertisement->get_DataSections(&sections)) || FAILED(sections->get_Size(&section_count)))
        return;
    for (UINT32 i = 0; i < section_count; ++i)
    {
        ComPtr<IBluetoothLEAdvertisementDataSection> section;
        ComPtr<ABI::Windows::Storage::Streams::IBuffer> buffer;
        BYTE data_type = 0;
        if (FAILED(sections->GetAt(i, &section)) || FAILED(section->get_DataType(&data_type))
            || FAILED(section->get_Data(&buffer)))
            continue;

        const size_t uuid_size = data_type == 0x16 ? 2 : data_type == 0x20 ? 4 : data_type == 0x21 ? 16 : 0;
        std::vector<uint8_t> bytes = winrt::read_buffer(buffer.Get());
        if (uuid_size == 0 || bytes.size() < uuid_size)
            continue;
        result.service_data.insert_or_assign(service_uuid(bytes, uuid_size),
                                             std::vector<uint8_t>(bytes.begin() + uuid_size, bytes.end()));
    }
}

void read_advertisement(IBluetoothLEAdvertisement *advertisement, ScanResult & result)
{
    read_service_data(advertisement, result);

    Microsoft::WRL::Wrappers::HString name;
    if (SUCCEEDED(advertisement->get_LocalName(name.GetAddressOf())))
        result.identifier = winrt::to_utf8(name.Get());

    ComPtr<ABI::Windows::Foundation::Collections::IVector<GUID>> uuids;
    UINT32 uuid_count = 0;
    if (SUCCEEDED(advertisement->get_ServiceUuids(&uuids)) && SUCCEEDED(uuids->get_Size(&uuid_count)))
    {
        for (UINT32 i = 0; i < uuid_count; ++i)
        {
            GUID uuid {};
            if (SUCCEEDED(uuids->GetAt(i, &uuid)))
                result.uuids.emplace_back(winrt::format_uuid(uuid));
        }
    }

    ComPtr<ABI::Windows::Foundation::Collections::IVector<BluetoothLEManufacturerData *>> manufacturer_data;
    UINT32 data_count = 0;
    if (FAILED(advertisement->get_ManufacturerData(&manufacturer_data))
        || FAILED(manufacturer_data->get_Size(&data_count)))
        return;
    for (UINT32 i = 0; i < data_count; ++i)
    {
        ComPtr<IBluetoothLEManufacturerData> data;
        ComPtr<ABI::Windows::Storage::Streams::IBuffer> buffer;
        UINT16 company_id = 0;
        if (SUCCEEDED(manufacturer_data->GetAt(i, &data)) && SUCCEEDED(data->get_CompanyId(&company_id))
            && SUCCEEDED(data->get_Data(&buffer)))
            result.manufacturer_data.insert_or_assign(company_id, winrt::read_buffer(buffer.Get()));
    }
}

// windows 10 2004+: address type, tx power, connectable
void read_extended_args(IBluetoothLEAdvertisementReceivedEventArgs *args, ScanResult & result)
{
    ComPtr<IBluetoothLEAdvertisementReceivedEventArgs2> args2;
    if (FAILED(args->QueryInterface(IID_PPV_ARGS(&args2))))
        return;

    ABI::Windows::Devices::Bluetooth::BluetoothAddressType address_type {};
    if (SUCCEEDED(args2->get_BluetoothAddressType(&address_type)))
    {
        result.address_type = address_type == ABI::Windows::Devices::Bluetooth::BluetoothAddressType_Public
                                  ? AddressType::public_address
                                  : AddressType::random;
    }

    ComPtr<ABI::Windows::Foundation::IReference<INT16>> tx_power;
    INT16 tx_power_value = 0;
    if (SUCCEEDED(args2->get_TransmitPowerLevelInDBm(&tx_power)) && tx_power != nullptr
        && SUCCEEDED(tx_power->get_Value(&tx_power_value)))
        result.tx_power = tx_power_value;

    boolean connectable = 0;
    if (SUCCEEDED(args2->get_IsConnectable(&connectable)))
        result.connectable = connectable != 0;
}

std::optional<ScanResult> read_received(IBluetoothLEAdvertisementReceivedEventArgs *args)
{
    ScanResult result;
    UINT64 address = 0;
    INT16 rssi = 0;
    if (FAILED(args->get_BluetoothAddress(&address)) || FAILED(args->get_RawSignalStrengthInDBm(&rssi)))
        return std::nullopt;
    result.address = format_address(address);
    result.rssi = rssi;

    ComPtr<IBluetoothLEAdvertisement> advertisement;
    if (SUCCEEDED(args->get_Advertisement(&advertisement)))
        read_advertisement(advertisement.Get(), result);
    read_extended_args(args, result);
    return result;
}

HRESULT STDMETHODCALLTYPE ReceivedHandler::Invoke(IBluetoothLEAdvertisementWatcher *,
                                                  IBluetoothLEAdvertisementReceivedEventArgs *args)
{
    std::optional<ScanResult> result = read_received(args);
    if (!result)
        return S_OK;
    {
        auto lock = _waitable.guard();
        _pending.emplace_back(std::move(*result));
    }
    _waitable.notify_all();
    return S_OK;
}

Result<ComPtr<IBluetoothLEAdvertisementWatcher>> activate_watcher()
{
    auto watcher = winrt::activate<IBluetoothLEAdvertisementWatcher>(
        RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementWatcher);
    SIHD_UNEXPECTED_RETURN(watcher);
    const HRESULT hr = (*watcher)->put_ScanningMode(BluetoothLEScanningMode_Active);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "BLE active scanning"));
    return watcher;
}

ULONGLONG ticks_of(const SYSTEMTIME & time)
{
    FILETIME file_time {};
    SystemTimeToFileTime(&time, &file_time);
    return (static_cast<ULONGLONG>(file_time.dwHighDateTime) << 32) | file_time.dwLowDateTime;
}

ScanResult classic_result(const BLUETOOTH_DEVICE_INFO & info)
{
    ScanResult result;
    result.address = format_address(info.Address.ullLong);
    result.address_type = AddressType::public_address;
    result.identifier = str::to_str(std::wstring_view(info.szName));
    result.class_of_device = info.ulClassofDevice;
    return result;
}

// one classic inquiry: the devices it heard, remembered ones out of range keep an older last seen
std::vector<ScanResult> inquire()
{
    SYSTEMTIME started {};
    GetSystemTime(&started);
    BLUETOOTH_DEVICE_SEARCH_PARAMS params {};
    params.dwSize = sizeof(params);
    params.fReturnAuthenticated = TRUE;
    params.fReturnRemembered = TRUE;
    params.fReturnUnknown = TRUE;
    params.fReturnConnected = TRUE;
    params.fIssueInquiry = TRUE;
    params.cTimeoutMultiplier = inquiry_multiplier;

    std::vector<ScanResult> heard;
    BLUETOOTH_DEVICE_INFO info {};
    info.dwSize = sizeof(info);
    HBLUETOOTH_DEVICE_FIND find = BluetoothFindFirstDevice(&params, &info);
    for (bool found = find != nullptr; found; found = BluetoothFindNextDevice(find, &info) != FALSE)
    {
        if (info.fConnected || ticks_of(info.stLastSeen) >= ticks_of(started))
            heard.emplace_back(classic_result(info));
        info.dwSize = sizeof(info);
    }
    if (find != nullptr)
        BluetoothFindDeviceClose(find);
    return heard;
}

// the filter windows cannot apply: its watcher requires every uuid, any one is enough here
bool passes(const ScanFilter & filter, const ScanResult & result)
{
    if (filter.min_rssi.has_value() && result.rssi < *filter.min_rssi)
        return false;
    if (filter.uuids.empty())
        return true;
    return std::ranges::any_of(filter.uuids, [&result](const std::string & wanted) {
        const std::string uuid = str::to_lower_copy(wanted);
        return std::ranges::any_of(result.uuids, [&uuid](const std::string & advertised) { return advertised == uuid; })
               || result.service_data.contains(uuid);
    });
}

} // namespace

bool Scanner::_scan()
{
    if (_adapter_index != 0)
    {
        SIHD_LOG(error, "Scanner: windows scans with the default adapter only");
        return false;
    }
    winrt::Apartment apartment;
    if (SIHD_UNEXPECTED_LOG(apartment.entered()))
        return false;

    std::vector<ScanResult> pending;
    ReceivedHandler handler(_waitable, pending);
    ComPtr<IBluetoothLEAdvertisementWatcher> watcher;
    EventRegistrationToken token {};
    bool started = true;
    if (_filter.transport != Transport::classic)
    {
        auto activated = activate_watcher();
        started = !SIHD_UNEXPECTED_LOG(activated) && SUCCEEDED((*activated)->add_Received(&handler, &token))
                  && SUCCEEDED((*activated)->Start());
        if (activated)
            watcher = *activated;
    }

    std::atomic<bool> inquiring = started && _filter.transport != Transport::le;
    std::thread inquiries;
    if (inquiring)
    {
        inquiries = std::thread([this, &inquiring, &pending] {
            while (inquiring)
            {
                std::vector<ScanResult> heard = inquire();
                {
                    auto lock = _waitable.guard();
                    std::ranges::move(heard, std::back_inserter(pending));
                }
                _waitable.notify_all();
            }
        });
    }
    if (started)
        this->service_set_ready();
    else
        SIHD_LOG(error, "Scanner: could not start the BLE watcher");

    while (started)
    {
        this->_forget_lost();
        auto wait = this->_next_wait(max_wait);
        if (!wait.has_value())
            break;

        std::vector<ScanResult> received;
        {
            auto lock = _waitable.wait_for_guard(*wait, [this, &pending] {
                return this->service_stop_requested() || !pending.empty();
            });
            received.swap(pending);
        }
        for (ScanResult & result : received)
        {
            if (passes(_filter, result))
                this->_publish(std::move(result));
        }
        if (this->service_stop_requested())
            break;
    }

    inquiring = false;
    if (inquiries.joinable())
        inquiries.join();
    if (watcher)
    {
        watcher->remove_Received(token);
        watcher->Stop();
        watcher.Reset();
    }
    _waitable.wait([&handler] { return handler.released_by_others(); });
    return started;
}

} // namespace sihd::bt
