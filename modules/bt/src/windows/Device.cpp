#include <charconv>
#include <map>
#include <memory>
#include <mutex>

#include <fmt/format.h>

#include <sihd/bt/Device.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/time.hpp>

#include "../address.hpp"
#include "winrt.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;
using Microsoft::WRL::Wrappers::HString;
using winrt::ComPtr;
namespace foundation = ABI::Windows::Foundation;
namespace collections = ABI::Windows::Foundation::Collections;
namespace streams = ABI::Windows::Storage::Streams;
namespace bluetooth = ABI::Windows::Devices::Bluetooth;
namespace gatt = ABI::Windows::Devices::Bluetooth::GenericAttributeProfile;
namespace rfcomm = ABI::Windows::Devices::Bluetooth::Rfcomm;
namespace enumeration = ABI::Windows::Devices::Enumeration;

SIHD_LOGGER;

namespace
{

const Duration call_timeout(time::sec(10));
const Duration connect_timeout(time::sec(30));
const Duration pair_timeout(time::sec(60));
const Duration max_wait(time::hours(1));

using ConnectionHandler = foundation::ITypedEventHandler<bluetooth::BluetoothLEDevice *, IInspectable *>;
using ValueHandler = foundation::ITypedEventHandler<gatt::GattCharacteristic *, gatt::GattValueChangedEventArgs *>;
using PairingHandlerAbi = foundation::ITypedEventHandler<enumeration::DeviceInformationCustomPairing *,
                                                         enumeration::DevicePairingRequestedEventArgs *>;

constexpr auto answered_kinds = static_cast<enumeration::DevicePairingKinds>(
    enumeration::DevicePairingKinds_ConfirmOnly | enumeration::DevicePairingKinds_DisplayPin
    | enumeration::DevicePairingKinds_ProvidePin | enumeration::DevicePairingKinds_ConfirmPinMatch);

Error not_connected()
{
    return Error(closed, "bluetooth device is not connected");
}

Result<void> default_adapter(size_t index)
{
    if (index != 0)
        return std::unexpected(Error(not_supported, "windows reaches devices through its default adapter only"));
    return {};
}

// alive while one of the windows runtime handlers is
struct Link
{
        Waitable waitable;
        // guarded by waitable
        bool connected = false;
        bool lost = false;
        bool woken = false;
        ComPtr<IAsyncInfo> attempt;

        // cleared before the connection ends: a handler holding the mutex finishes first
        std::mutex notify_mutex;
        std::function<void(GattNotification &&)> notify;

        void set_connected(bool now)
        {
            {
                auto lock = waitable.guard();
                lost = lost || (connected && !now);
                connected = now;
            }
            waitable.notify_all();
        }
};

struct Subscription
{
        ComPtr<gatt::IGattCharacteristic> characteristic;
        EventRegistrationToken token;
};

// windows knows a device as LE, classic or both: LE first
struct Remote
{
        ComPtr<bluetooth::IBluetoothLEDevice> le;
        ComPtr<bluetooth::IBluetoothDevice> classic;
        ComPtr<enumeration::IDeviceInformation> information;
};

template <typename Logical, typename Statics, size_t Size>
Result<winrt::Awaited<Logical>> from_address(const wchar_t (&class_name)[Size], UINT64 address)
{
    auto statics = winrt::factory<Statics>(class_name);
    SIHD_UNEXPECTED_RETURN(statics);
    ComPtr<foundation::IAsyncOperation<Logical>> opening;
    const HRESULT hr = (*statics)->FromBluetoothAddressAsync(address, &opening);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth device"));
    return winrt::await(opening, call_timeout, "bluetooth device");
}

template <typename Second, typename Device>
ComPtr<enumeration::IDeviceInformation> information_of(const ComPtr<Device> & device)
{
    ComPtr<Second> second;
    ComPtr<enumeration::IDeviceInformation> information;
    if (device && SUCCEEDED(device.As(&second)))
        second->get_DeviceInformation(&information);
    return information;
}

Result<Remote> remote_of(const std::string & address)
{
    auto value = parse_address(address);
    SIHD_UNEXPECTED_RETURN(value);
    Remote remote;
    auto le = from_address<bluetooth::BluetoothLEDevice *, bluetooth::IBluetoothLEDeviceStatics>(
        RuntimeClass_Windows_Devices_Bluetooth_BluetoothLEDevice,
        *value);
    SIHD_UNEXPECTED_RETURN(le);
    remote.information = information_of<bluetooth::IBluetoothLEDevice2>(*le);
    if (remote.information)
    {
        remote.le = *le;
        return remote;
    }

    auto classic = from_address<bluetooth::BluetoothDevice *, bluetooth::IBluetoothDeviceStatics>(
        RuntimeClass_Windows_Devices_Bluetooth_BluetoothDevice,
        *value);
    SIHD_UNEXPECTED_RETURN(classic);
    remote.information = information_of<bluetooth::IBluetoothDevice2>(*classic);
    if (!remote.information)
        return std::unexpected(Error(not_found, "windows does not know {}", address));
    remote.classic = *classic;
    return remote;
}

Result<ComPtr<enumeration::IDeviceInformationPairing>> pairing_of(enumeration::IDeviceInformation *information)
{
    ComPtr<enumeration::IDeviceInformation2> second;
    ComPtr<enumeration::IDeviceInformationPairing> pairing;
    HRESULT hr = information->QueryInterface(IID_PPV_ARGS(&second));
    if (SUCCEEDED(hr))
        hr = second->get_Pairing(&pairing);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "bluetooth pairing"));
    return pairing;
}

template <typename Answer>
Result<void> status_of(Answer *answer, std::string_view what)
{
    gatt::GattCommunicationStatus status = gatt::GattCommunicationStatus_Unreachable;
    ComPtr<foundation::IReference<BYTE>> protocol_error;
    answer->get_Status(&status);
    answer->get_ProtocolError(&protocol_error);
    return winrt::gatt_status(status, protocol_error.Get(), what);
}

Result<void> written(const ComPtr<foundation::IAsyncOperation<gatt::GattWriteResult *>> & writing,
                     std::string_view what)
{
    auto result = winrt::await(writing, call_timeout, what);
    SIHD_UNEXPECTED_RETURN(result);
    return status_of(result->Get(), what);
}

Result<std::vector<uint8_t>> read_value(const ComPtr<foundation::IAsyncOperation<gatt::GattReadResult *>> & reading,
                                        std::string_view what)
{
    auto read = winrt::await(reading, call_timeout, what);
    SIHD_UNEXPECTED_RETURN(read);
    gatt::GattCommunicationStatus status = gatt::GattCommunicationStatus_Unreachable;
    ComPtr<gatt::IGattReadResult2> second;
    ComPtr<foundation::IReference<BYTE>> protocol_error;
    ComPtr<streams::IBuffer> value;
    (*read)->get_Status(&status);
    if (SUCCEEDED(read->As(&second)))
        second->get_ProtocolError(&protocol_error);
    auto reached = winrt::gatt_status(status, protocol_error.Get(), what);
    SIHD_UNEXPECTED_RETURN(reached);
    (*read)->get_Value(&value);
    return winrt::read_buffer(value.Get());
}

// with a link, a stop cancels the listing in flight
Result<std::vector<ComPtr<gatt::IGattDeviceService>>> services_of(bluetooth::IBluetoothLEDevice *device,
                                                                  bluetooth::BluetoothCacheMode mode,
                                                                  Link *link,
                                                                  const std::function<bool()> & stopping)
{
    ComPtr<bluetooth::IBluetoothLEDevice3> third;
    ComPtr<foundation::IAsyncOperation<gatt::GattDeviceServicesResult *>> listing;
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&third));
    if (SUCCEEDED(hr))
        hr = third->GetGattServicesWithCacheModeAsync(mode, &listing);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt services"));

    if (link != nullptr)
    {
        auto lock = link->waitable.guard();
        listing.As(&link->attempt);
        if (stopping())
            link->attempt->Cancel();
    }
    auto listed = winrt::await(listing, connect_timeout, "gatt services");
    if (link != nullptr)
    {
        auto lock = link->waitable.guard();
        link->attempt.Reset();
    }
    SIHD_UNEXPECTED_RETURN(listed);

    auto reached = status_of(listed->Get(), "gatt services");
    SIHD_UNEXPECTED_RETURN(reached);
    ComPtr<collections::IVectorView<gatt::GattDeviceService *>> view;
    (*listed)->get_Services(&view);
    return winrt::items<gatt::IGattDeviceService>(view.Get());
}

Result<std::vector<ComPtr<gatt::IGattCharacteristic>>> characteristics_of(gatt::IGattDeviceService *service)
{
    ComPtr<gatt::IGattDeviceService3> third;
    ComPtr<foundation::IAsyncOperation<gatt::GattCharacteristicsResult *>> listing;
    HRESULT hr = service->QueryInterface(IID_PPV_ARGS(&third));
    if (SUCCEEDED(hr))
        hr = third->GetCharacteristicsWithCacheModeAsync(bluetooth::BluetoothCacheMode_Uncached, &listing);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt characteristics"));
    auto listed = winrt::await(listing, call_timeout, "gatt characteristics");
    SIHD_UNEXPECTED_RETURN(listed);
    auto reached = status_of(listed->Get(), "gatt characteristics");
    SIHD_UNEXPECTED_RETURN(reached);
    ComPtr<collections::IVectorView<gatt::GattCharacteristic *>> view;
    (*listed)->get_Characteristics(&view);
    return winrt::items<gatt::IGattCharacteristic>(view.Get());
}

Result<std::vector<ComPtr<gatt::IGattDescriptor>>> descriptors_of(gatt::IGattCharacteristic *characteristic)
{
    ComPtr<gatt::IGattCharacteristic3> third;
    ComPtr<foundation::IAsyncOperation<gatt::GattDescriptorsResult *>> listing;
    HRESULT hr = characteristic->QueryInterface(IID_PPV_ARGS(&third));
    if (SUCCEEDED(hr))
        hr = third->GetDescriptorsWithCacheModeAsync(bluetooth::BluetoothCacheMode_Uncached, &listing);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt descriptors"));
    auto listed = winrt::await(listing, call_timeout, "gatt descriptors");
    SIHD_UNEXPECTED_RETURN(listed);
    auto reached = status_of(listed->Get(), "gatt descriptors");
    SIHD_UNEXPECTED_RETURN(reached);
    ComPtr<collections::IVectorView<gatt::GattDescriptor *>> view;
    (*listed)->get_Descriptors(&view);
    return winrt::items<gatt::IGattDescriptor>(view.Get());
}

Result<ComPtr<gatt::IGattSession>> open_session(bluetooth::IBluetoothLEDevice *device)
{
    auto ids = winrt::factory<bluetooth::IBluetoothDeviceIdStatics>(
        RuntimeClass_Windows_Devices_Bluetooth_BluetoothDeviceId);
    SIHD_UNEXPECTED_RETURN(ids);
    auto sessions = winrt::factory<gatt::IGattSessionStatics>(
        RuntimeClass_Windows_Devices_Bluetooth_GenericAttributeProfile_GattSession);
    SIHD_UNEXPECTED_RETURN(sessions);

    HString id;
    ComPtr<bluetooth::IBluetoothDeviceId> device_id;
    ComPtr<foundation::IAsyncOperation<gatt::GattSession *>> opening;
    HRESULT hr = device->get_DeviceId(id.GetAddressOf());
    if (SUCCEEDED(hr))
        hr = (*ids)->FromId(id.Get(), &device_id);
    if (SUCCEEDED(hr))
        hr = (*sessions)->FromDeviceIdAsync(device_id.Get(), &opening);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt session"));

    auto session = winrt::await(opening, call_timeout, "gatt session");
    SIHD_UNEXPECTED_RETURN(session);
    // windows connects on demand: the session keeps the link up
    hr = (*session)->put_MaintainConnection(TRUE);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt session"));
    return session;
}

// windows drops a link once the objects using it are closed
void close(IUnknown *object)
{
    ComPtr<foundation::IClosable> closable;
    if (object != nullptr && SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&closable))))
        closable->Close();
}

void describe_le(bluetooth::IBluetoothLEDevice *device, DeviceInfo & info)
{
    HString name;
    bluetooth::BluetoothConnectionStatus status = bluetooth::BluetoothConnectionStatus_Disconnected;
    device->get_Name(name.GetAddressOf());
    device->get_ConnectionStatus(&status);
    info.name = winrt::to_utf8(name.Get());
    info.connected = status == bluetooth::BluetoothConnectionStatus_Connected;

    ComPtr<bluetooth::IBluetoothLEDevice2> second;
    ComPtr<bluetooth::IBluetoothLEAppearance> appearance;
    bluetooth::BluetoothAddressType type = bluetooth::BluetoothAddressType_Unspecified;
    UINT16 raw_appearance = 0;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&second))))
    {
        second->get_BluetoothAddressType(&type);
        // windows reads an unknown appearance as 0
        if (SUCCEEDED(second->get_Appearance(&appearance)) && appearance
            && SUCCEEDED(appearance->get_RawValue(&raw_appearance)) && raw_appearance != 0)
            info.appearance = raw_appearance;
    }
    info.address_type = type == bluetooth::BluetoothAddressType_Public   ? AddressType::public_address
                        : type == bluetooth::BluetoothAddressType_Random ? AddressType::random
                                                                         : AddressType::unspecified;

    auto services = services_of(device, bluetooth::BluetoothCacheMode_Cached, nullptr, {});
    for (const ComPtr<gatt::IGattDeviceService> & service : services.value_or({}))
    {
        GUID uuid {};
        if (SUCCEEDED(service->get_Uuid(&uuid)))
            info.uuids.emplace_back(winrt::format_uuid(uuid));
        close(service.Get());
    }
}

void describe_classic(bluetooth::IBluetoothDevice *device, DeviceInfo & info)
{
    HString name;
    bluetooth::BluetoothConnectionStatus status = bluetooth::BluetoothConnectionStatus_Disconnected;
    device->get_Name(name.GetAddressOf());
    device->get_ConnectionStatus(&status);
    info.name = winrt::to_utf8(name.Get());
    info.connected = status == bluetooth::BluetoothConnectionStatus_Connected;
    info.address_type = AddressType::public_address;

    ComPtr<bluetooth::IBluetoothClassOfDevice> class_of_device;
    UINT32 raw_class = 0;
    if (SUCCEEDED(device->get_ClassOfDevice(&class_of_device)) && class_of_device
        && SUCCEEDED(class_of_device->get_RawValue(&raw_class)))
        info.class_of_device = raw_class;

    ComPtr<collections::IVectorView<rfcomm::RfcommDeviceService *>> services;
    if (FAILED(device->get_RfcommServices(&services)))
        return;
    for (const ComPtr<rfcomm::IRfcommDeviceService> & service :
         winrt::items<rfcomm::IRfcommDeviceService>(services.Get()))
    {
        ComPtr<rfcomm::IRfcommServiceId> id;
        GUID uuid {};
        if (SUCCEEDED(service->get_ServiceId(&id)) && SUCCEEDED(id->get_Uuid(&uuid)))
            info.uuids.emplace_back(winrt::format_uuid(uuid));
    }
}

Error pairing_error(enumeration::DevicePairingResultStatus status, const std::string & address)
{
    switch (status)
    {
        case enumeration::DevicePairingResultStatus_AlreadyPaired:
            return Error(already_exists, "{} is already paired", address);
        case enumeration::DevicePairingResultStatus_OperationAlreadyInProgress:
            return Error(would_block, "a pairing of {} is in progress", address);
        case enumeration::DevicePairingResultStatus_AuthenticationTimeout:
            return Error(timeout, "the pairing of {} timed out", address);
        case enumeration::DevicePairingResultStatus_ConnectionRejected:
        case enumeration::DevicePairingResultStatus_AuthenticationNotAllowed:
        case enumeration::DevicePairingResultStatus_AccessDenied:
        case enumeration::DevicePairingResultStatus_RejectedByHandler:
        case enumeration::DevicePairingResultStatus_PairingCanceled:
        case enumeration::DevicePairingResultStatus_ProtectionLevelCouldNotBeMet:
            return Error(permission_denied,
                         "the pairing of {} was refused: status {}",
                         address,
                         static_cast<int>(status));
        default:
            return Error(io_error, "the pairing of {} failed: status {}", address, static_cast<int>(status));
    }
}

// a displayed code needs no answer, as with bluez
void answer_pairing(const PairingHandler & handler,
                    const std::string & address,
                    bool le,
                    enumeration::IDevicePairingRequestedEventArgs *args)
{
    enumeration::DevicePairingKinds kind = enumeration::DevicePairingKinds_None;
    HString pin;
    args->get_PairingKind(&kind);
    args->get_Pin(pin.GetAddressOf());
    PairingRequest request {.address = address,
                            .kind = PairingRequest::Kind::confirm,
                            .passkey = 0,
                            .pin = winrt::to_utf8(pin.Get())};
    std::from_chars(request.pin.data(), request.pin.data() + request.pin.size(), request.passkey);
    if (kind == enumeration::DevicePairingKinds_ConfirmPinMatch)
        request.kind = PairingRequest::Kind::confirm_passkey;
    else if (kind == enumeration::DevicePairingKinds_DisplayPin)
        request.kind = le ? PairingRequest::Kind::display_passkey : PairingRequest::Kind::display_pin;
    else if (kind == enumeration::DevicePairingKinds_ProvidePin)
        request.kind = le ? PairingRequest::Kind::enter_passkey : PairingRequest::Kind::enter_pin;

    const PairingAnswer answer = handler(request);
    if (!answer.accept && kind != enumeration::DevicePairingKinds_DisplayPin)
        return;
    if (kind == enumeration::DevicePairingKinds_ProvidePin)
        args->AcceptWithPin(winrt::hstring_of(le ? fmt::format("{:06}", answer.passkey) : answer.pin).Get());
    else
        args->Accept();
}

} // namespace

struct Device::Impl
{
        std::shared_ptr<Link> link = std::make_shared<Link>();

        // the connection, other threads use it under calls_mutex
        std::mutex calls_mutex;
        ComPtr<bluetooth::IBluetoothLEDevice> device;
        ComPtr<gatt::IGattSession> session;
        std::vector<ComPtr<gatt::IGattDeviceService>> services;
        // from the last services()
        std::map<uint16_t, ComPtr<gatt::IGattCharacteristic>> characteristics;
        std::map<uint16_t, ComPtr<gatt::IGattDescriptor>> descriptors;
        std::map<uint16_t, Subscription> subscriptions;

        std::mutex pairing_mutex;
        ComPtr<IAsyncInfo> pairing;

        template <typename Attribute>
        Result<ComPtr<Attribute>> attribute(const std::map<uint16_t, ComPtr<Attribute>> & attributes, uint16_t handle)
        {
            if (!device)
                return std::unexpected(not_connected());
            const auto it = attributes.find(handle);
            if (it == attributes.end())
                return std::unexpected(
                    Error(not_found, "no gatt attribute of handle {}, services() lists them", handle));
            return it->second;
        }

        Result<void> configure(gatt::IGattCharacteristic *characteristic,
                               gatt::GattClientCharacteristicConfigurationDescriptorValue value)
        {
            ComPtr<gatt::IGattCharacteristic3> third;
            ComPtr<foundation::IAsyncOperation<gatt::GattWriteResult *>> writing;
            HRESULT hr = characteristic->QueryInterface(IID_PPV_ARGS(&third));
            if (SUCCEEDED(hr))
                hr = third->WriteClientCharacteristicConfigurationDescriptorWithResultAsync(value, &writing);
            if (FAILED(hr))
                return std::unexpected(winrt::error_of(hr, "subscription"));
            return written(writing, "subscription");
        }

        // calls_mutex held: the characteristic and its descriptors, their handles remembered
        Result<GattCharacteristic> list(const ComPtr<gatt::IGattCharacteristic> & characteristic)
        {
            GattCharacteristic attribute;
            GUID uuid {};
            gatt::GattCharacteristicProperties properties = gatt::GattCharacteristicProperties_None;
            characteristic->get_Uuid(&uuid);
            characteristic->get_AttributeHandle(&attribute.handle);
            characteristic->get_CharacteristicProperties(&properties);
            attribute.uuid = winrt::format_uuid(uuid);
            // the low byte holds the properties of the core specification
            attribute.properties = static_cast<uint8_t>(properties & 0xff);
            characteristics.insert_or_assign(attribute.handle, characteristic);

            auto listed = descriptors_of(characteristic.Get());
            SIHD_UNEXPECTED_RETURN(listed);
            for (const ComPtr<gatt::IGattDescriptor> & descriptor : *listed)
            {
                GattDescriptor & described = attribute.descriptors.emplace_back();
                descriptor->get_Uuid(&uuid);
                descriptor->get_AttributeHandle(&described.handle);
                described.uuid = winrt::format_uuid(uuid);
                descriptors.insert_or_assign(described.handle, descriptor);
            }
            return attribute;
        }

        // calls_mutex held
        void disconnect()
        {
            for (const auto & [handle, subscription] : subscriptions)
                subscription.characteristic->remove_ValueChanged(subscription.token);
            subscriptions.clear();
            characteristics.clear();
            descriptors.clear();
            for (const ComPtr<gatt::IGattDeviceService> & service : services)
                close(service.Get());
            services.clear();
            if (session)
                session->put_MaintainConnection(FALSE);
            close(session.Get());
            session.Reset();
            close(device.Get());
            device.Reset();
        }
};

Device::Device(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>())
{
    this->_add_confs();
}

Device::~Device()
{
    if (this->is_running())
        this->stop();
}

Result<DeviceInfo> Device::info() const
{
    auto adapter = default_adapter(_adapter_index);
    SIHD_UNEXPECTED_RETURN(adapter);
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto remote = remote_of(_address);
    SIHD_UNEXPECTED_RETURN(remote);
    auto pairing = pairing_of(remote->information.Get());
    SIHD_UNEXPECTED_RETURN(pairing);

    DeviceInfo info;
    info.address = _address;
    boolean paired = FALSE;
    (*pairing)->get_IsPaired(&paired);
    info.paired = paired != FALSE;
    if (remote->le)
        describe_le(remote->le.Get(), info);
    else
        describe_classic(remote->classic.Get(), info);
    return info;
}

Result<void> Device::pair()
{
    auto adapter = default_adapter(_adapter_index);
    SIHD_UNEXPECTED_RETURN(adapter);
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto remote = remote_of(_address);
    SIHD_UNEXPECTED_RETURN(remote);
    auto pairing = pairing_of(remote->information.Get());
    SIHD_UNEXPECTED_RETURN(pairing);

    ComPtr<foundation::IAsyncOperation<enumeration::DevicePairingResult *>> operation;
    ComPtr<enumeration::IDeviceInformationCustomPairing> custom;
    EventRegistrationToken token {};
    HRESULT hr = S_OK;
    if (_pairing_handler)
    {
        ComPtr<enumeration::IDeviceInformationPairing2> second;
        auto requested = winrt::delegate<PairingHandlerAbi,
                                         enumeration::IDeviceInformationCustomPairing *,
                                         enumeration::IDevicePairingRequestedEventArgs *>(
            [handler = _pairing_handler, address = _address, le = remote->le != nullptr](
                enumeration::IDeviceInformationCustomPairing *,
                enumeration::IDevicePairingRequestedEventArgs *args) { answer_pairing(handler, address, le, args); });
        hr = pairing->As(&second);
        if (SUCCEEDED(hr))
            hr = second->get_Custom(&custom);
        if (SUCCEEDED(hr))
            hr = custom->add_PairingRequested(requested.Get(), &token);
        if (SUCCEEDED(hr))
            hr = custom->PairAsync(answered_kinds, &operation);
    }
    else
        hr = (*pairing)->PairAsync(&operation);

    std::optional<Result<ComPtr<enumeration::IDevicePairingResult>>> result;
    if (SUCCEEDED(hr))
    {
        {
            std::lock_guard lock(_impl->pairing_mutex);
            operation.As(&_impl->pairing);
        }
        result = winrt::await(operation, pair_timeout, "pairing");
        std::lock_guard lock(_impl->pairing_mutex);
        _impl->pairing.Reset();
    }
    if (custom && token.value != 0)
        custom->remove_PairingRequested(token);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "pairing"));
    if (!*result && result->error().code == interrupted)
        return std::unexpected(Error(permission_denied, "the pairing of {} was canceled", _address));
    SIHD_UNEXPECTED_RETURN(*result);

    enumeration::DevicePairingResultStatus status = enumeration::DevicePairingResultStatus_Failed;
    (**result)->get_Status(&status);
    if (status != enumeration::DevicePairingResultStatus_Paired)
        return std::unexpected(pairing_error(status, _address));
    return {};
}

Result<void> Device::cancel_pairing()
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    std::lock_guard lock(_impl->pairing_mutex);
    if (!_impl->pairing)
        return std::unexpected(Error(not_found, "no pairing of {} in progress", _address));
    const HRESULT hr = _impl->pairing->Cancel();
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "pairing cancel"));
    return {};
}

Result<void> Device::unpair()
{
    auto adapter = default_adapter(_adapter_index);
    SIHD_UNEXPECTED_RETURN(adapter);
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto remote = remote_of(_address);
    SIHD_UNEXPECTED_RETURN(remote);
    auto pairing = pairing_of(remote->information.Get());
    SIHD_UNEXPECTED_RETURN(pairing);

    ComPtr<enumeration::IDeviceInformationPairing2> second;
    ComPtr<foundation::IAsyncOperation<enumeration::DeviceUnpairingResult *>> operation;
    HRESULT hr = pairing->As(&second);
    if (SUCCEEDED(hr))
        hr = second->UnpairAsync(&operation);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "unpairing"));
    auto result = winrt::await(operation, pair_timeout, "unpairing");
    SIHD_UNEXPECTED_RETURN(result);

    enumeration::DeviceUnpairingResultStatus status = enumeration::DeviceUnpairingResultStatus_Failed;
    (*result)->get_Status(&status);
    switch (status)
    {
        // like bluez removing a device it knows unpaired
        case enumeration::DeviceUnpairingResultStatus_Unpaired:
        case enumeration::DeviceUnpairingResultStatus_AlreadyUnpaired:
            return {};
        case enumeration::DeviceUnpairingResultStatus_OperationAlreadyInProgress:
            return std::unexpected(Error(would_block, "an unpairing of {} is in progress", _address));
        case enumeration::DeviceUnpairingResultStatus_AccessDenied:
            return std::unexpected(Error(permission_denied, "the unpairing of {} was refused", _address));
        default:
            return std::unexpected(Error(io_error, "the unpairing of {} failed", _address));
    }
}

Result<void> Device::set_trusted(bool)
{
    return std::unexpected(Error(not_supported, "windows has no trusted devices"));
}

Result<std::vector<GattService>> Device::services()
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    std::lock_guard lock(_impl->calls_mutex);
    if (!_impl->device)
        return std::unexpected(not_connected());

    _impl->characteristics.clear();
    _impl->descriptors.clear();
    std::vector<GattService> listed;
    for (const ComPtr<gatt::IGattDeviceService> & service : _impl->services)
    {
        GattService & described = listed.emplace_back();
        GUID uuid {};
        service->get_Uuid(&uuid);
        service->get_AttributeHandle(&described.handle);
        described.uuid = winrt::format_uuid(uuid);

        auto characteristics = characteristics_of(service.Get());
        SIHD_UNEXPECTED_RETURN(characteristics);
        for (const ComPtr<gatt::IGattCharacteristic> & characteristic : *characteristics)
        {
            auto attribute = _impl->list(characteristic);
            SIHD_UNEXPECTED_RETURN(attribute);
            described.characteristics.emplace_back(std::move(*attribute));
        }
    }
    return listed;
}

Result<std::vector<uint8_t>> Device::read_characteristic(const GattCharacteristic & characteristic)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    std::lock_guard lock(_impl->calls_mutex);
    auto found = _impl->attribute(_impl->characteristics, characteristic.handle);
    SIHD_UNEXPECTED_RETURN(found);
    ComPtr<foundation::IAsyncOperation<gatt::GattReadResult *>> reading;
    const HRESULT hr = (*found)->ReadValueWithCacheModeAsync(bluetooth::BluetoothCacheMode_Uncached, &reading);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "read"));
    return read_value(reading, "read");
}

Result<void> Device::write_characteristic(const GattCharacteristic & characteristic, ArrUByteView value, WriteType type)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto buffer = winrt::make_buffer(value);
    SIHD_UNEXPECTED_RETURN(buffer);
    std::lock_guard lock(_impl->calls_mutex);
    auto found = _impl->attribute(_impl->characteristics, characteristic.handle);
    SIHD_UNEXPECTED_RETURN(found);

    ComPtr<gatt::IGattCharacteristic3> third;
    ComPtr<foundation::IAsyncOperation<gatt::GattWriteResult *>> writing;
    HRESULT hr = found->As(&third);
    if (SUCCEEDED(hr))
        hr = third->WriteValueWithResultAndOptionAsync(buffer->Get(),
                                                       type == WriteType::with_response
                                                           ? gatt::GattWriteOption_WriteWithResponse
                                                           : gatt::GattWriteOption_WriteWithoutResponse,
                                                       &writing);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "write"));
    return written(writing, "write");
}

Result<std::vector<uint8_t>> Device::read_descriptor(const GattDescriptor & descriptor)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    std::lock_guard lock(_impl->calls_mutex);
    auto found = _impl->attribute(_impl->descriptors, descriptor.handle);
    SIHD_UNEXPECTED_RETURN(found);
    ComPtr<foundation::IAsyncOperation<gatt::GattReadResult *>> reading;
    const HRESULT hr = (*found)->ReadValueWithCacheModeAsync(bluetooth::BluetoothCacheMode_Uncached, &reading);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "descriptor read"));
    return read_value(reading, "descriptor read");
}

Result<void> Device::write_descriptor(const GattDescriptor & descriptor, ArrUByteView value)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto buffer = winrt::make_buffer(value);
    SIHD_UNEXPECTED_RETURN(buffer);
    std::lock_guard lock(_impl->calls_mutex);
    auto found = _impl->attribute(_impl->descriptors, descriptor.handle);
    SIHD_UNEXPECTED_RETURN(found);

    ComPtr<gatt::IGattDescriptor2> second;
    ComPtr<foundation::IAsyncOperation<gatt::GattWriteResult *>> writing;
    HRESULT hr = found->As(&second);
    if (SUCCEEDED(hr))
        hr = second->WriteValueWithResultAsync(buffer->Get(), &writing);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "descriptor write"));
    return written(writing, "descriptor write");
}

Result<void> Device::subscribe(const GattCharacteristic & characteristic)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    std::lock_guard lock(_impl->calls_mutex);
    auto found = _impl->attribute(_impl->characteristics, characteristic.handle);
    SIHD_UNEXPECTED_RETURN(found);
    if (_impl->subscriptions.contains(characteristic.handle))
        return {};

    gatt::GattCharacteristicProperties properties = gatt::GattCharacteristicProperties_None;
    (*found)->get_CharacteristicProperties(&properties);
    const bool notifies = (properties & gatt::GattCharacteristicProperties_Notify) != 0;
    if (!notifies && (properties & gatt::GattCharacteristicProperties_Indicate) == 0)
        return std::unexpected(
            Error(not_supported, "characteristic {} neither notifies nor indicates", characteristic.uuid));

    // a notification may come before the CCCD write answers: listened first
    auto changed = winrt::delegate<ValueHandler, gatt::IGattCharacteristic *, gatt::IGattValueChangedEventArgs *>(
        [link = _impl->link](gatt::IGattCharacteristic *sender, gatt::IGattValueChangedEventArgs *args) {
            GattNotification notification;
            GUID uuid {};
            ComPtr<streams::IBuffer> value;
            sender->get_AttributeHandle(&notification.handle);
            sender->get_Uuid(&uuid);
            args->get_CharacteristicValue(&value);
            notification.uuid = winrt::format_uuid(uuid);
            notification.value = winrt::read_buffer(value.Get());
            std::lock_guard notify_lock(link->notify_mutex);
            if (link->notify)
                link->notify(std::move(notification));
        });

    EventRegistrationToken token {};
    const HRESULT hr = (*found)->add_ValueChanged(changed.Get(), &token);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "subscription"));
    auto configured = _impl->configure(found->Get(),
                                       notifies ? gatt::GattClientCharacteristicConfigurationDescriptorValue_Notify
                                                : gatt::GattClientCharacteristicConfigurationDescriptorValue_Indicate);
    if (!configured)
        (*found)->remove_ValueChanged(token);
    SIHD_UNEXPECTED_RETURN(configured);
    _impl->subscriptions.insert_or_assign(characteristic.handle,
                                          Subscription {.characteristic = *found, .token = token});
    return {};
}

Result<void> Device::unsubscribe(const GattCharacteristic & characteristic)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    std::lock_guard lock(_impl->calls_mutex);
    auto found = _impl->attribute(_impl->characteristics, characteristic.handle);
    SIHD_UNEXPECTED_RETURN(found);
    if (auto it = _impl->subscriptions.find(characteristic.handle); it != _impl->subscriptions.end())
    {
        (*found)->remove_ValueChanged(it->second.token);
        _impl->subscriptions.erase(it);
    }
    return _impl->configure(found->Get(), gatt::GattClientCharacteristicConfigurationDescriptorValue_None);
}

Result<uint16_t> Device::mtu()
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    std::lock_guard lock(_impl->calls_mutex);
    if (!_impl->session)
        return std::unexpected(not_connected());
    UINT16 size = 0;
    const HRESULT hr = _impl->session->get_MaxPduSize(&size);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "mtu"));
    return size;
}

bool Device::on_start()
{
    if (SIHD_UNEXPECTED_LOG(default_adapter(_adapter_index)))
        return false;
    winrt::Apartment apartment;
    if (SIHD_UNEXPECTED_LOG(apartment.entered()))
        return false;
    auto address = parse_address(_address);
    if (SIHD_UNEXPECTED_LOG(address))
        return false;
    auto opened = from_address<bluetooth::BluetoothLEDevice *, bluetooth::IBluetoothLEDeviceStatics>(
        RuntimeClass_Windows_Devices_Bluetooth_BluetoothLEDevice,
        *address);
    if (SIHD_UNEXPECTED_LOG(opened))
        return false;
    if (!*opened)
    {
        SIHD_LOG(error, "Device: windows does not know {}", _address);
        return false;
    }
    const ComPtr<bluetooth::IBluetoothLEDevice> device = *opened;

    Link & link = *_impl->link;
    {
        auto lock = link.waitable.guard();
        link.connected = false;
        link.lost = false;
        link.woken = false;
    }
    {
        std::lock_guard lock(link.notify_mutex);
        link.notify = [this](GattNotification && notification) {
            this->_queue_notification(std::move(notification));
        };
    }
    auto status_changed = winrt::delegate<ConnectionHandler, bluetooth::IBluetoothLEDevice *, IInspectable *>(
        [link = _impl->link](bluetooth::IBluetoothLEDevice *sender, IInspectable *) {
            bluetooth::BluetoothConnectionStatus status = bluetooth::BluetoothConnectionStatus_Disconnected;
            sender->get_ConnectionStatus(&status);
            link->set_connected(status == bluetooth::BluetoothConnectionStatus_Connected);
        });
    EventRegistrationToken status_token {};
    const bool following = SUCCEEDED(device->add_ConnectionStatusChanged(status_changed.Get(), &status_token));

    auto session = open_session(device.Get());
    auto services = session ? services_of(device.Get(),
                                          bluetooth::BluetoothCacheMode_Uncached,
                                          &link,
                                          [this] { return this->service_stop_requested(); })
                            : std::unexpected(session.error());
    bool connected = false;
    if (following && !SIHD_UNEXPECTED_LOG(services))
    {
        bluetooth::BluetoothConnectionStatus status = bluetooth::BluetoothConnectionStatus_Disconnected;
        device->get_ConnectionStatus(&status);
        link.set_connected(status == bluetooth::BluetoothConnectionStatus_Connected);
        auto lock = link.waitable.wait_for_guard(connect_timeout, [this, &link] {
            return link.connected || link.lost || this->service_stop_requested();
        });
        connected = link.connected && !link.lost;
    }
    if (!connected && !this->service_stop_requested())
        SIHD_LOG(error, "Device: cannot connect {}", _address);

    {
        std::lock_guard lock(_impl->calls_mutex);
        _impl->device = device;
        _impl->session = session.value_or(nullptr);
        _impl->services = services.value_or({});
    }
    if (connected)
    {
        this->_publish_event(Event::connected);
        this->service_set_ready();
    }
    while (connected && !this->service_stop_requested())
    {
        this->_flush_notifications();
        auto lock = link.waitable.wait_for_guard(max_wait, [this, &link] {
            return link.lost || link.woken || this->service_stop_requested();
        });
        link.woken = false;
        if (link.lost)
            break;
    }

    {
        std::lock_guard lock(link.notify_mutex);
        link.notify = nullptr;
    }
    if (following)
        device->remove_ConnectionStatusChanged(status_token);
    {
        std::lock_guard lock(_impl->calls_mutex);
        _impl->disconnect();
    }
    {
        std::lock_guard lock(_pending_mutex);
        _pending.clear();
    }

    const bool stopped = this->service_stop_requested();
    if (connected)
        this->_publish_event(Event::disconnected);
    return connected && stopped;
}

void Device::_wake_loop()
{
    {
        auto lock = _impl->link->waitable.guard();
        _impl->link->woken = true;
    }
    _impl->link->waitable.notify_all();
}

bool Device::on_stop()
{
    {
        // the service flag is already set: an attempt stored after this cancels itself
        auto lock = _impl->link->waitable.guard();
        if (_impl->link->attempt)
            _impl->link->attempt->Cancel();
    }
    this->_wake_loop();
    return true;
}

} // namespace sihd::bt
