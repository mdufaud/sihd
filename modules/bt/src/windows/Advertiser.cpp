#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <utility>

#include <sihd/bt/Advertiser.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

#include "winrt.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;
using winrt::ComPtr;
namespace foundation = ABI::Windows::Foundation;
namespace collections = ABI::Windows::Foundation::Collections;
namespace bluetooth = ABI::Windows::Devices::Bluetooth;
namespace advertisement = ABI::Windows::Devices::Bluetooth::Advertisement;

SIHD_LOGGER;

namespace
{

const Duration call_timeout(time::sec(10));
// AD types of the service data with a 16, 32 and 128 bits uuid
constexpr BYTE service_data_16 = 0x16;
constexpr BYTE service_data_32 = 0x20;
constexpr BYTE service_data_128 = 0x21;
// the bluetooth base uuid 00000000-0000-1000-8000-00805f9b34fb past its first 32 bits
constexpr unsigned short base_data2 = 0x0000;
constexpr unsigned short base_data3 = 0x1000;
constexpr std::array<unsigned char, 8> base_data4 = {0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb};

using StatusHandler = foundation::ITypedEventHandler<
    advertisement::BluetoothLEAdvertisementPublisher *,
    advertisement::BluetoothLEAdvertisementPublisherStatusChangedEventArgs *>;

struct PublisherState
{
        Waitable waitable;
        // guarded by waitable
        advertisement::BluetoothLEAdvertisementPublisherStatus
            status = advertisement::BluetoothLEAdvertisementPublisherStatus_Created;
        bluetooth::BluetoothError error = bluetooth::BluetoothError_Success;
};

void append_little_endian(std::vector<uint8_t> & bytes, uint64_t value, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

// the AD type and payload of a service data: the uuid in its shortest little endian form, then the data
Result<std::pair<BYTE, std::vector<uint8_t>>> service_data_section(const std::string & uuid,
                                                                   const std::vector<uint8_t> & data)
{
    auto guid = winrt::guid_of(uuid);
    SIHD_UNEXPECTED_RETURN(guid);

    std::vector<uint8_t> payload;
    BYTE type = service_data_128;
    if (guid->Data2 == base_data2 && guid->Data3 == base_data3 && std::ranges::equal(guid->Data4, base_data4))
    {
        type = guid->Data1 <= 0xffff ? service_data_16 : service_data_32;
        append_little_endian(payload, guid->Data1, type == service_data_16 ? 2 : 4);
    }
    else
    {
        payload.insert(payload.end(), std::rbegin(guid->Data4), std::rend(guid->Data4));
        append_little_endian(payload, guid->Data3, 2);
        append_little_endian(payload, guid->Data2, 2);
        append_little_endian(payload, guid->Data1, 4);
    }

    payload.insert(payload.end(), data.begin(), data.end());
    return std::make_pair(type, std::move(payload));
}

Result<void> add_manufacturer_data(advertisement::IBluetoothLEAdvertisement *built, const Advertisement & content)
{
    auto factory = winrt::factory<advertisement::IBluetoothLEManufacturerDataFactory>(
        RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEManufacturerData);
    SIHD_UNEXPECTED_RETURN(factory);

    ComPtr<collections::IVector<advertisement::BluetoothLEManufacturerData *>> entries;
    HRESULT hr = built->get_ManufacturerData(&entries);
    for (const auto & [company, data] : content.manufacturer_data)
    {
        auto buffer = winrt::make_buffer(data);
        SIHD_UNEXPECTED_RETURN(buffer);
        ComPtr<advertisement::IBluetoothLEManufacturerData> entry;
        if (SUCCEEDED(hr))
            hr = (*factory)->Create(company, buffer->Get(), &entry);
        if (SUCCEEDED(hr))
            hr = entries->Append(entry.Get());
    }
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth manufacturer data"));
    return {};
}

Result<void> add_service_data(advertisement::IBluetoothLEAdvertisement *built, const Advertisement & content)
{
    auto factory = winrt::factory<advertisement::IBluetoothLEAdvertisementDataSectionFactory>(
        RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementDataSection);
    SIHD_UNEXPECTED_RETURN(factory);

    ComPtr<collections::IVector<advertisement::BluetoothLEAdvertisementDataSection *>> sections;
    HRESULT hr = built->get_DataSections(&sections);
    for (const auto & [uuid, data] : content.service_data)
    {
        auto section = service_data_section(uuid, data);
        SIHD_UNEXPECTED_RETURN(section);
        auto buffer = winrt::make_buffer(section->second);
        SIHD_UNEXPECTED_RETURN(buffer);
        ComPtr<advertisement::IBluetoothLEAdvertisementDataSection> entry;
        if (SUCCEEDED(hr))
            hr = (*factory)->Create(section->first, buffer->Get(), &entry);
        if (SUCCEEDED(hr))
            hr = sections->Append(entry.Get());
    }
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth service data"));
    return {};
}

Result<ComPtr<advertisement::IBluetoothLEAdvertisementPublisher>> create_publisher(const Advertisement & content)
{
    auto built = winrt::activate<advertisement::IBluetoothLEAdvertisement>(
        RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisement);
    SIHD_UNEXPECTED_RETURN(built);
    auto manufacturer_data = add_manufacturer_data(built->Get(), content);
    SIHD_UNEXPECTED_RETURN(manufacturer_data);
    auto service_data = add_service_data(built->Get(), content);
    SIHD_UNEXPECTED_RETURN(service_data);
    auto publishers = winrt::factory<advertisement::IBluetoothLEAdvertisementPublisherFactory>(
        RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementPublisher);
    SIHD_UNEXPECTED_RETURN(publishers);
    ComPtr<advertisement::IBluetoothLEAdvertisementPublisher> publisher;
    const HRESULT hr = (*publishers)->Create(built->Get(), &publisher);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth advertising"));
    return publisher;
}

} // namespace

struct Advertiser::Impl
{
        std::shared_ptr<PublisherState> state = std::make_shared<PublisherState>();
};

Advertiser::Advertiser(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>())
{
    this->set_adapter(0);
    this->add_conf("adapter", &Advertiser::set_adapter);
}

Advertiser::~Advertiser()
{
    if (this->is_running())
        this->stop();
}

bool Advertiser::on_start()
{
    if (_adapter_index != 0)
    {
        SIHD_LOG(error, "Advertiser: windows advertises through its default adapter only");
        return false;
    }
    if (!_advertisement.local_name.empty() || !_advertisement.service_uuids.empty() || _advertisement.connectable)
    {
        SIHD_LOG(
            error,
            "Advertiser: windows advertises a name, service uuids or a connectable device through a GattServer only");
        return false;
    }
    winrt::Apartment apartment;
    if (SIHD_UNEXPECTED_LOG(apartment.entered()))
        return false;
    auto publisher = create_publisher(_advertisement);
    if (SIHD_UNEXPECTED_LOG(publisher))
        return false;

    PublisherState & state = *_impl->state;
    {
        auto lock = state.waitable.guard();
        state.status = advertisement::BluetoothLEAdvertisementPublisherStatus_Created;
        state.error = bluetooth::BluetoothError_Success;
    }
    auto changed = winrt::delegate<StatusHandler,
                                   advertisement::IBluetoothLEAdvertisementPublisher *,
                                   advertisement::IBluetoothLEAdvertisementPublisherStatusChangedEventArgs *>(
        [shared = _impl->state](advertisement::IBluetoothLEAdvertisementPublisher *,
                                advertisement::IBluetoothLEAdvertisementPublisherStatusChangedEventArgs *args) {
            advertisement::BluetoothLEAdvertisementPublisherStatus
                status = advertisement::BluetoothLEAdvertisementPublisherStatus_Aborted;
            bluetooth::BluetoothError error = bluetooth::BluetoothError_OtherError;
            args->get_Status(&status);
            args->get_Error(&error);
            {
                auto lock = shared->waitable.guard();
                shared->status = status;
                shared->error = error;
            }
            shared->waitable.notify_all();
        });

    EventRegistrationToken token {};
    HRESULT hr = (*publisher)->add_StatusChanged(changed.Get(), &token);
    if (SUCCEEDED(hr))
        hr = (*publisher)->Start();
    if (FAILED(hr))
        SIHD_LOG(error, "Advertiser: {}", winrt::error_of(hr, "bluetooth advertising").message);

    bool advertised = false;
    if (SUCCEEDED(hr))
    {
        auto lock = state.waitable.wait_for_guard(call_timeout, [this, &state] {
            return state.status == advertisement::BluetoothLEAdvertisementPublisherStatus_Started
                   || state.status == advertisement::BluetoothLEAdvertisementPublisherStatus_Aborted
                   || this->service_stop_requested();
        });
        advertised = state.status == advertisement::BluetoothLEAdvertisementPublisherStatus_Started;
    }
    if (advertised)
    {
        this->service_set_ready();
        state.waitable.wait([this, &state] {
            return state.status == advertisement::BluetoothLEAdvertisementPublisherStatus_Aborted
                   || this->service_stop_requested();
        });
    }
    {
        auto lock = state.waitable.guard();
        if (state.status == advertisement::BluetoothLEAdvertisementPublisherStatus_Aborted)
            SIHD_LOG(error, "Advertiser: {}", winrt::bluetooth_error(state.error, "bluetooth advertising").message);
    }
    (*publisher)->Stop();
    (*publisher)->remove_StatusChanged(token);
    return advertised && this->service_stop_requested();
}

bool Advertiser::on_stop()
{
    {
        // the stop request is visible to the waiting on_start() once it holds the guard
        auto lock = _impl->state->waitable.guard();
    }
    _impl->state->waitable.notify_all();
    return true;
}

} // namespace sihd::bt
