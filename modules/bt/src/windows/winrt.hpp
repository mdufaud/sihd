#ifndef __SIHD_BT_WINDOWS_WINRT_HPP__
#define __SIHD_BT_WINDOWS_WINRT_HPP__

#include <initguid.h>
#include <windows.h>

// mingw-w64 declares IReference<boolean> beside IReference<BYTE>, the same type in C++
#define ____FIReference_1_boolean_INTERFACE_DEFINED__

#include <roapi.h>
#include <robuffer.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <windows.devices.bluetooth.advertisement.h>
#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <windows.devices.bluetooth.h>
#include <windows.devices.bluetooth.rfcomm.h>
#include <windows.devices.enumeration.h>
#include <windows.devices.radios.h>
#include <windows.storage.streams.h>

#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/Waitable.hpp>

#include "winrt_abi.hpp"

namespace sihd::bt::winrt
{

namespace foundation = ABI::Windows::Foundation;
template <typename Interface>
using ComPtr = Microsoft::WRL::ComPtr<Interface>;

// the multithreaded apartment of the calling thread for its scope
class Apartment
{
    public:
        Apartment();
        ~Apartment();

        Apartment(const Apartment &) = delete;
        Apartment & operator=(const Apartment &) = delete;

        sihd::util::Result<void> entered() const;

    private:
        HRESULT _hr;
};

sihd::util::Error error_of(HRESULT hr, std::string_view what);

std::string to_utf8(HSTRING hstring);
Microsoft::WRL::Wrappers::HString hstring_of(std::string_view text);

std::string format_uuid(const GUID & guid);
sihd::util::Result<GUID> guid_of(const std::string & uuid);

std::vector<uint8_t> read_buffer(ABI::Windows::Storage::Streams::IBuffer *buffer);
sihd::util::Result<ComPtr<ABI::Windows::Storage::Streams::IBuffer>> make_buffer(sihd::util::ArrUByteView bytes);

template <typename Interface, size_t Size>
sihd::util::Result<ComPtr<Interface>> factory(const wchar_t (&class_name)[Size])
{
    ComPtr<Interface> found;
    const HRESULT hr = RoGetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(class_name).Get(),
                                              IID_PPV_ARGS(&found));
    if (FAILED(hr))
        return std::unexpected(error_of(hr, "windows runtime class " + std::string(class_name, class_name + Size - 1)));
    return found;
}

template <typename Interface, size_t Size>
sihd::util::Result<ComPtr<Interface>> activate(const wchar_t (&class_name)[Size])
{
    ComPtr<IInspectable> inspectable;
    HRESULT hr = RoActivateInstance(Microsoft::WRL::Wrappers::HStringReference(class_name).Get(), &inspectable);
    ComPtr<Interface> activated;
    if (SUCCEEDED(hr))
        hr = inspectable.As(&activated);
    if (FAILED(hr))
        return std::unexpected(error_of(hr, "windows runtime class " + std::string(class_name, class_name + Size - 1)));
    return activated;
}

// a handler the windows runtime calls from its threads, freed with its last reference
template <typename Interface>
class Unknown: public Interface
{
    public:
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
        {
            if (riid == IID_IUnknown || riid == IID_IAgileObject || riid == __uuidof(Interface))
            {
                *object = static_cast<Interface *>(this);
                this->AddRef();
                return S_OK;
            }
            *object = nullptr;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override { return ++_references; }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const ULONG references = --_references;
            if (references == 0)
                delete this;
            return references;
        }

    protected:
        virtual ~Unknown() = default;

    private:
        std::atomic<ULONG> _references = 1;
};

template <typename Interface, typename... Args>
class Delegate final: public Unknown<Interface>
{
    public:
        explicit Delegate(std::function<void(Args...)> call): _call(std::move(call)) {}

        HRESULT STDMETHODCALLTYPE Invoke(Args... args) override
        {
            _call(args...);
            return S_OK;
        }

    private:
        std::function<void(Args...)> _call;
};

template <typename Interface, typename... Args>
ComPtr<Interface> delegate(std::type_identity_t<std::function<void(Args...)>> call)
{
    ComPtr<Interface> handler;
    handler.Attach(new Delegate<Interface, Args...>(std::move(call)));
    return handler;
}

// the elements of an IVectorView, the ones it fails to give skipped
template <typename Element, typename View>
std::vector<ComPtr<Element>> items(View *view)
{
    std::vector<ComPtr<Element>> elements;
    UINT32 size = 0;
    if (view == nullptr || FAILED(view->get_Size(&size)))
        return elements;
    for (UINT32 i = 0; i < size; ++i)
    {
        ComPtr<Element> element;
        if (SUCCEEDED(view->GetAt(i, &element)))
            elements.emplace_back(std::move(element));
    }
    return elements;
}

// the result type an IAsyncOperation hands over: a reference for runtime classes, a value otherwise
template <typename Logical>
using AbiOf = typename foundation::Internal::GetAbiType<
    typename foundation::IAsyncOperation<Logical>::TResult_complex>::type;

template <typename Logical>
using Awaited = std::
    conditional_t<std::is_pointer_v<AbiOf<Logical>>, ComPtr<std::remove_pointer_t<AbiOf<Logical>>>, AbiOf<Logical>>;

template <typename Logical>
class Completion final: public Unknown<foundation::IAsyncOperationCompletedHandler<Logical>>
{
    public:
        HRESULT STDMETHODCALLTYPE Invoke(foundation::IAsyncOperation<Logical> *, AsyncStatus status) override
        {
            {
                auto lock = _waitable.guard();
                _status = status;
            }
            _waitable.notify_all();
            return S_OK;
        }

        std::optional<AsyncStatus> wait(sihd::util::Duration timeout)
        {
            auto lock = _waitable.wait_for_guard(timeout, [this] { return _status.has_value(); });
            return _status;
        }

    private:
        sihd::util::Waitable _waitable;
        std::optional<AsyncStatus> _status;
};

// the error of an operation that ended without results, canceled when it timed out
sihd::util::Error async_error(IInspectable *operation, std::optional<AsyncStatus> status, std::string_view what);

template <typename Logical>
sihd::util::Result<Awaited<Logical>> await(const ComPtr<foundation::IAsyncOperation<Logical>> & operation,
                                           sihd::util::Duration timeout,
                                           std::string_view what)
{
    ComPtr<Completion<Logical>> completion;
    completion.Attach(new Completion<Logical>());
    HRESULT hr = operation->put_Completed(completion.Get());
    if (FAILED(hr))
        return std::unexpected(error_of(hr, what));
    const std::optional<AsyncStatus> status = completion->wait(timeout);
    if (status != AsyncStatus::Completed)
        return std::unexpected(async_error(operation.Get(), status, what));

    Awaited<Logical> results {};
    if constexpr (std::is_pointer_v<AbiOf<Logical>>)
        hr = operation->GetResults(results.ReleaseAndGetAddressOf());
    else
        hr = operation->GetResults(&results);
    if (FAILED(hr))
        return std::unexpected(error_of(hr, what));
    return results;
}

sihd::util::Error bluetooth_error(ABI::Windows::Devices::Bluetooth::BluetoothError error, std::string_view what);

sihd::util::Result<void>
    gatt_status(ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus status,
                foundation::IReference<BYTE> *protocol_error,
                std::string_view what);

} // namespace sihd::bt::winrt

#endif
