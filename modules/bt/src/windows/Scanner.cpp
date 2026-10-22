#include <initguid.h>
#include <windows.h>

// mingw-w64 declares IReference<boolean> beside IReference<BYTE>, the same type in C++
#define ____FIReference_1_boolean_INTERFACE_DEFINED__

#include <roapi.h>
#include <robuffer.h>

#include <atomic>
#include <optional>
#include <vector>
#include <windows.devices.bluetooth.advertisement.h>
#include <windows.storage.streams.h>

#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <sihd/bt/Scanner.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

namespace sihd::bt
{

using enum sihd::util::ErrorCode;
using namespace sihd::util;
using namespace ABI::Windows::Devices::Bluetooth::Advertisement;
using Microsoft::WRL::ComPtr;

SIHD_LOGGER;

namespace
{

using ReceivedEventHandler = ABI::Windows::Foundation::ITypedEventHandler<BluetoothLEAdvertisementWatcher *,
                                                                          BluetoothLEAdvertisementReceivedEventArgs *>;

const Duration max_wait(time::milli(500));

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

std::string format_address(UINT64 address)
{
    return fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                       (address >> 40) & 0xff,
                       (address >> 32) & 0xff,
                       (address >> 24) & 0xff,
                       (address >> 16) & 0xff,
                       (address >> 8) & 0xff,
                       address & 0xff);
}

std::string format_uuid(const GUID & guid)
{
    return fmt::format("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                       guid.Data1,
                       guid.Data2,
                       guid.Data3,
                       guid.Data4[0],
                       guid.Data4[1],
                       guid.Data4[2],
                       guid.Data4[3],
                       guid.Data4[4],
                       guid.Data4[5],
                       guid.Data4[6],
                       guid.Data4[7]);
}

std::string to_utf8(HSTRING hstring)
{
    UINT32 length = 0;
    const wchar_t *buffer = WindowsGetStringRawBuffer(hstring, &length);
    return str::to_str(std::wstring_view(buffer, length));
}

std::vector<uint8_t> read_buffer(ABI::Windows::Storage::Streams::IBuffer *buffer)
{
    ComPtr<::Windows::Storage::Streams::IBufferByteAccess> byte_access;
    UINT32 length = 0;
    byte *data = nullptr;
    if (FAILED(buffer->QueryInterface(IID_PPV_ARGS(&byte_access))) || FAILED(buffer->get_Length(&length))
        || FAILED(byte_access->Buffer(&data)))
        return {};
    return std::vector<uint8_t>(data, data + length);
}

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
        std::vector<uint8_t> bytes = read_buffer(buffer.Get());
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
        result.identifier = to_utf8(name.Get());

    ComPtr<ABI::Windows::Foundation::Collections::IVector<GUID>> uuids;
    UINT32 uuid_count = 0;
    if (SUCCEEDED(advertisement->get_ServiceUuids(&uuids)) && SUCCEEDED(uuids->get_Size(&uuid_count)))
    {
        for (UINT32 i = 0; i < uuid_count; ++i)
        {
            GUID uuid {};
            if (SUCCEEDED(uuids->GetAt(i, &uuid)))
                result.uuids.emplace_back(format_uuid(uuid));
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
            result.manufacturer_data.insert_or_assign(company_id, read_buffer(buffer.Get()));
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
    Microsoft::WRL::Wrappers::HStringReference class_name(
        RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementWatcher);
    ComPtr<IInspectable> inspectable;
    HRESULT hr = RoActivateInstance(class_name.Get(), &inspectable);
    if (FAILED(hr))
        return std::unexpected(Error(io_error, "could not activate the BLE watcher: 0x{:08x}", static_cast<ULONG>(hr)));

    ComPtr<IBluetoothLEAdvertisementWatcher> watcher;
    hr = inspectable.As(&watcher);
    if (FAILED(hr))
        return std::unexpected(Error(io_error, "not a BLE watcher: 0x{:08x}", static_cast<ULONG>(hr)));
    hr = watcher->put_ScanningMode(BluetoothLEScanningMode_Active);
    if (FAILED(hr))
        return std::unexpected(Error(io_error, "could not set active scanning: 0x{:08x}", static_cast<ULONG>(hr)));
    return watcher;
}

} // namespace

Result<bool> bluetooth_enabled()
{
    return std::unexpected(Error(not_supported, "bluetooth_enabled is not implemented on windows"));
}

Result<std::vector<AdapterInfo>> list_adapters()
{
    return std::unexpected(Error(not_supported, "list_adapters is not implemented on windows"));
}

bool Scanner::_scan()
{
    if (_adapter_index != 0)
    {
        SIHD_LOG(error, "Scanner: windows scans with the default adapter only");
        return false;
    }
    Microsoft::WRL::Wrappers::RoInitializeWrapper ro_initialized(RO_INIT_MULTITHREADED);
    if (FAILED(ro_initialized) && static_cast<HRESULT>(ro_initialized) != RPC_E_CHANGED_MODE)
    {
        SIHD_LOG(error, "Scanner: could not initialize the windows runtime");
        return false;
    }
    auto watcher = activate_watcher();
    if (SIHD_UNEXPECTED_LOG(watcher))
        return false;

    std::vector<ScanResult> pending;
    ReceivedHandler handler(_waitable, pending);
    EventRegistrationToken token {};
    const bool started = SUCCEEDED((*watcher)->add_Received(&handler, &token)) && SUCCEEDED((*watcher)->Start());
    if (started)
        this->service_set_ready();
    else
        SIHD_LOG(error, "Scanner: could not start the BLE watcher");

    while (started)
    {
        auto wait = this->_next_wait(max_wait);
        if (!wait.has_value())
            break;

        std::vector<ScanResult> received;
        {
            auto lock = _waitable.wait_for_guard(*wait, [this, &pending] {
                return _stop_requested.load() || !pending.empty();
            });
            received.swap(pending);
        }
        for (ScanResult & result : received)
            this->_publish(std::move(result));
        if (_stop_requested)
            break;
    }

    (*watcher)->remove_Received(token);
    (*watcher)->Stop();
    watcher->Reset();
    _waitable.wait([&handler] { return handler.released_by_others(); });
    return started;
}

} // namespace sihd::bt
