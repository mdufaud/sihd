#include <algorithm>
#include <memory>
#include <mutex>

#include <sihd/bt/GattServer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

#include "winrt.hpp"

namespace sihd::bt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;
using winrt::ComPtr;
namespace foundation = ABI::Windows::Foundation;
namespace streams = ABI::Windows::Storage::Streams;
namespace bluetooth = ABI::Windows::Devices::Bluetooth;
namespace gatt = ABI::Windows::Devices::Bluetooth::GenericAttributeProfile;

SIHD_LOGGER;

namespace
{

const Duration call_timeout(time::sec(10));
// the ATT errors the server answers
constexpr BYTE write_not_permitted = 0x03;
constexpr BYTE unlikely_error = 0x0e;

using ReadHandler = foundation::ITypedEventHandler<gatt::GattLocalCharacteristic *, gatt::GattReadRequestedEventArgs *>;
using WriteHandler = foundation::ITypedEventHandler<gatt::GattLocalCharacteristic *,
                                                    gatt::GattWriteRequestedEventArgs *>;

struct Published
{
        std::string service_uuid;
        LocalCharacteristic characteristic;
        std::vector<uint8_t> value;
        ComPtr<gatt::IGattLocalCharacteristic> local;
        EventRegistrationToken read_token {};
        EventRegistrationToken write_token {};
};

// what the request handlers of one start() reach: closed when it ends, once the handlers in flight returned
struct Sink
{
        Waitable waitable;
        // guarded by waitable, the published list keeps its size
        bool active = true;
        int running = 0;
        std::vector<Published> published;

        Published *find(const std::string & service_uuid, const std::string & characteristic_uuid)
        {
            auto it = std::ranges::find_if(published, [&](const Published & candidate) {
                return str::to_lower_copy(candidate.service_uuid) == str::to_lower_copy(service_uuid)
                       && str::to_lower_copy(candidate.characteristic.uuid) == str::to_lower_copy(characteristic_uuid);
            });
            return it != published.end() ? &*it : nullptr;
        }

        std::optional<std::vector<uint8_t>> read(size_t index)
        {
            std::function<std::vector<uint8_t>()> on_read;
            std::vector<uint8_t> value;
            {
                auto lock = waitable.guard();
                if (!active)
                    return std::nullopt;
                ++running;
                on_read = published[index].characteristic.on_read;
                value = published[index].value;
            }
            if (on_read)
                value = on_read();
            this->leave(nullptr, {});
            return value;
        }

        bool write(size_t index, std::vector<uint8_t> && value)
        {
            std::function<bool(const std::vector<uint8_t> &)> on_write;
            {
                auto lock = waitable.guard();
                if (!active)
                    return false;
                ++running;
                on_write = published[index].characteristic.on_write;
            }
            const bool accepted = !on_write || on_write(value);
            this->leave(accepted ? &published[index] : nullptr, std::move(value));
            return accepted;
        }

        void leave(Published *written, std::vector<uint8_t> && value)
        {
            {
                auto lock = waitable.guard();
                --running;
                if (written != nullptr)
                    written->value = std::move(value);
            }
            waitable.notify_all();
        }

        void close()
        {
            {
                auto lock = waitable.guard();
                active = false;
            }
            waitable.wait([this] { return running == 0; });
        }
};

void answer_read(Sink & sink, size_t index, gatt::IGattReadRequestedEventArgs *args)
{
    ComPtr<foundation::IDeferral> deferral;
    ComPtr<foundation::IAsyncOperation<gatt::GattReadRequest *>> requesting;
    if (FAILED(args->GetDeferral(&deferral)) || FAILED(args->GetRequestAsync(&requesting)))
        return;
    auto request = winrt::await(requesting, call_timeout, "gatt read request");
    if (request && *request)
    {
        const std::optional<std::vector<uint8_t>> value = sink.read(index);
        auto buffer = value.has_value() ? winrt::make_buffer(*value) : std::unexpected(Error(closed, "gatt server"));
        if (buffer)
            (*request)->RespondWithValue(buffer->Get());
        else
            (*request)->RespondWithProtocolError(unlikely_error);
    }
    deferral->Complete();
}

void answer_write(Sink & sink, size_t index, gatt::IGattWriteRequestedEventArgs *args)
{
    ComPtr<foundation::IDeferral> deferral;
    ComPtr<foundation::IAsyncOperation<gatt::GattWriteRequest *>> requesting;
    if (FAILED(args->GetDeferral(&deferral)) || FAILED(args->GetRequestAsync(&requesting)))
        return;
    auto request = winrt::await(requesting, call_timeout, "gatt write request");
    if (request && *request)
    {
        ComPtr<streams::IBuffer> value;
        gatt::GattWriteOption option = gatt::GattWriteOption_WriteWithResponse;
        (*request)->get_Value(&value);
        (*request)->get_Option(&option);

        const bool accepted = sink.write(index, winrt::read_buffer(value.Get()));
        // a write without response gets no answer
        if (option == gatt::GattWriteOption_WriteWithResponse && accepted)
            (*request)->Respond();
        else if (option == gatt::GattWriteOption_WriteWithResponse)
            (*request)->RespondWithProtocolError(write_not_permitted);
    }
    deferral->Complete();
}

Result<void> add_characteristic(gatt::IGattLocalService *service, const std::shared_ptr<Sink> & sink, size_t index)
{
    Published & published = sink->published[index];
    auto uuid = winrt::guid_of(published.characteristic.uuid);
    SIHD_UNEXPECTED_RETURN(uuid);
    auto parameters = winrt::activate<gatt::IGattLocalCharacteristicParameters>(
        RuntimeClass_Windows_Devices_Bluetooth_GenericAttributeProfile_GattLocalCharacteristicParameters);
    SIHD_UNEXPECTED_RETURN(parameters);

    ComPtr<foundation::IAsyncOperation<gatt::GattLocalCharacteristicResult *>> creating;
    HRESULT hr = (*parameters)
                     ->put_CharacteristicProperties(
                         static_cast<gatt::GattCharacteristicProperties>(published.characteristic.properties));
    if (SUCCEEDED(hr))
        hr = service->CreateCharacteristicAsync(*uuid, parameters->Get(), &creating);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt characteristic"));
    auto created = winrt::await(creating, call_timeout, "gatt characteristic");
    SIHD_UNEXPECTED_RETURN(created);
    bluetooth::BluetoothError error = bluetooth::BluetoothError_OtherError;
    (*created)->get_Error(&error);
    (*created)->get_Characteristic(&published.local);
    if (error != bluetooth::BluetoothError_Success || !published.local)
        return std::unexpected(winrt::bluetooth_error(error, "gatt characteristic"));

    auto read = winrt::delegate<ReadHandler, gatt::IGattLocalCharacteristic *, gatt::IGattReadRequestedEventArgs *>(
        [sink, index](gatt::IGattLocalCharacteristic *, gatt::IGattReadRequestedEventArgs *args) {
            answer_read(*sink, index, args);
        });
    auto write = winrt::delegate<WriteHandler, gatt::IGattLocalCharacteristic *, gatt::IGattWriteRequestedEventArgs *>(
        [sink, index](gatt::IGattLocalCharacteristic *, gatt::IGattWriteRequestedEventArgs *args) {
            answer_write(*sink, index, args);
        });
    hr = published.local->add_ReadRequested(read.Get(), &published.read_token);
    if (SUCCEEDED(hr))
        hr = published.local->add_WriteRequested(write.Get(), &published.write_token);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt characteristic"));
    return {};
}

// a provider per service, advertising its uuid once started
Result<ComPtr<gatt::IGattServiceProvider>>
    provide(const LocalService & service, const std::shared_ptr<Sink> & sink, size_t first)
{
    if (!service.primary)
        return std::unexpected(Error(not_supported, "windows publishes primary services only"));
    auto uuid = winrt::guid_of(service.uuid);
    SIHD_UNEXPECTED_RETURN(uuid);
    auto providers = winrt::factory<gatt::IGattServiceProviderStatics>(
        RuntimeClass_Windows_Devices_Bluetooth_GenericAttributeProfile_GattServiceProvider);
    SIHD_UNEXPECTED_RETURN(providers);
    ComPtr<foundation::IAsyncOperation<gatt::GattServiceProviderResult *>> creating;
    const HRESULT hr = (*providers)->CreateAsync(*uuid, &creating);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt service"));
    auto created = winrt::await(creating, call_timeout, "gatt service");
    SIHD_UNEXPECTED_RETURN(created);

    bluetooth::BluetoothError error = bluetooth::BluetoothError_OtherError;
    ComPtr<gatt::IGattServiceProvider> provider;
    ComPtr<gatt::IGattLocalService> local;
    (*created)->get_Error(&error);
    (*created)->get_ServiceProvider(&provider);
    if (error != bluetooth::BluetoothError_Success || !provider || FAILED(provider->get_Service(&local)))
        return std::unexpected(winrt::bluetooth_error(error, "gatt service"));
    for (size_t i = 0; i < service.characteristics.size(); ++i)
    {
        auto added = add_characteristic(local.Get(), sink, first + i);
        SIHD_UNEXPECTED_RETURN(added);
    }
    return provider;
}

Result<void> advertise(gatt::IGattServiceProvider *provider)
{
    auto parameters = winrt::activate<gatt::IGattServiceProviderAdvertisingParameters>(
        RuntimeClass_Windows_Devices_Bluetooth_GenericAttributeProfile_GattServiceProviderAdvertisingParameters);
    SIHD_UNEXPECTED_RETURN(parameters);
    HRESULT hr = (*parameters)->put_IsConnectable(TRUE);
    if (SUCCEEDED(hr))
        hr = (*parameters)->put_IsDiscoverable(TRUE);
    if (SUCCEEDED(hr))
        hr = provider->StartAdvertisingWithParameters(parameters->Get());
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt service advertising"));
    return {};
}

} // namespace

struct GattServer::Impl
{
        Waitable waitable;
        std::mutex sink_mutex;
        std::shared_ptr<Sink> sink;
};

GattServer::GattServer(const std::string & name, Node *parent): Named(name, parent), _impl(std::make_unique<Impl>())
{
    this->set_adapter(0);
    this->add_conf("adapter", &GattServer::set_adapter);
}

GattServer::~GattServer()
{
    if (this->is_running())
        this->stop();
}

Result<void>
    GattServer::notify(const std::string & service_uuid, const std::string & characteristic_uuid, ArrUByteView value)
{
    winrt::Apartment apartment;
    auto entered = apartment.entered();
    SIHD_UNEXPECTED_RETURN(entered);
    auto buffer = winrt::make_buffer(value);
    SIHD_UNEXPECTED_RETURN(buffer);

    ComPtr<gatt::IGattLocalCharacteristic> local;
    {
        std::lock_guard sink_lock(_impl->sink_mutex);
        if (_impl->sink == nullptr)
            return std::unexpected(Error(closed, "gatt server is not started"));
        auto lock = _impl->sink->waitable.guard();
        Published *found = _impl->sink->find(service_uuid, characteristic_uuid);
        if (found == nullptr)
            return std::unexpected(
                Error(not_found, "no characteristic {} in service {}", characteristic_uuid, service_uuid));
        if ((found->characteristic.properties & (GattCharacteristic::notify | GattCharacteristic::indicate)) == 0)
            return std::unexpected(
                Error(invalid_argument, "characteristic {} neither notifies nor indicates", characteristic_uuid));
        found->value.assign(value.begin(), value.end());
        local = found->local;
    }

    ComPtr<foundation::IAsyncOperation<foundation::Collections::IVectorView<gatt::GattClientNotificationResult *> *>>
        notifying;
    const HRESULT hr = local->NotifyValueAsync(buffer->Get(), &notifying);
    if (FAILED(hr))
        return std::unexpected(winrt::error_of(hr, "gatt notification"));
    auto notified = winrt::await(notifying, call_timeout, "gatt notification");
    SIHD_UNEXPECTED_RETURN(notified);
    return {};
}

bool GattServer::on_start()
{
    if (_adapter_index != 0)
    {
        SIHD_LOG(error, "GattServer: windows serves through its default adapter only");
        return false;
    }
    winrt::Apartment apartment;
    if (SIHD_UNEXPECTED_LOG(apartment.entered()))
        return false;

    auto sink = std::make_shared<Sink>();
    for (const LocalService & service : _services)
    {
        for (const LocalCharacteristic & characteristic : service.characteristics)
            sink->published.push_back(Published {.service_uuid = service.uuid,
                                                 .characteristic = characteristic,
                                                 .value = {},
                                                 .local = {},
                                                 .read_token = {},
                                                 .write_token = {}});
    }
    std::vector<ComPtr<gatt::IGattServiceProvider>> providers;
    bool served = true;
    size_t first = 0;
    for (const LocalService & service : _services)
    {
        auto provider = provide(service, sink, first);
        served = !SIHD_UNEXPECTED_LOG(provider);
        if (!served)
            break;
        providers.emplace_back(std::move(*provider));
        first += service.characteristics.size();
    }
    for (size_t i = 0; served && i < providers.size(); ++i)
        served = !SIHD_UNEXPECTED_LOG(advertise(providers[i].Get()));

    if (served)
    {
        {
            std::lock_guard lock(_impl->sink_mutex);
            _impl->sink = sink;
        }
        this->service_set_ready();
        _impl->waitable.wait([this] { return this->service_stop_requested(); });
        std::lock_guard lock(_impl->sink_mutex);
        _impl->sink.reset();
    }

    for (const ComPtr<gatt::IGattServiceProvider> & provider : providers)
        provider->StopAdvertising();
    for (const Published & published : sink->published)
    {
        if (!published.local)
            continue;
        published.local->remove_ReadRequested(published.read_token);
        published.local->remove_WriteRequested(published.write_token);
    }
    sink->close();
    return served;
}

bool GattServer::on_stop()
{
    {
        // the stop request is visible to the waiting on_start() once it holds the guard
        auto lock = _impl->waitable.guard();
    }
    _impl->waitable.notify_all();
    return true;
}

} // namespace sihd::bt
