#include "winrt.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>

#include <fmt/format.h>

#include <sihd/sys/os.hpp>
#include <sihd/util/str.hpp>

#include "../att.hpp"

namespace sihd::bt::winrt
{

using namespace sihd::util;
using enum sihd::util::ErrorCode;
namespace streams = ABI::Windows::Storage::Streams;
namespace gatt = ABI::Windows::Devices::Bluetooth::GenericAttributeProfile;
namespace bluetooth = ABI::Windows::Devices::Bluetooth;

namespace
{

// HRESULT_FROM_WIN32 of ERROR_NOT_FOUND, _TIMEOUT, _SEM_TIMEOUT, _DEVICE_NOT_CONNECTED, _NOT_SUPPORTED
constexpr HRESULT not_found_hr = static_cast<HRESULT>(0x80070490);
constexpr HRESULT timeout_hr = static_cast<HRESULT>(0x800705b4);
constexpr HRESULT sem_timeout_hr = static_cast<HRESULT>(0x80070079);
constexpr HRESULT not_connected_hr = static_cast<HRESULT>(0x8007048f);
constexpr HRESULT not_supported_hr = static_cast<HRESULT>(0x80070032);
// the object was closed
constexpr HRESULT closed_hr = static_cast<HRESULT>(0x80000013);

ErrorCode code_of(HRESULT hr)
{
    switch (hr)
    {
        case E_ACCESSDENIED:
            return permission_denied;
        case E_INVALIDARG:
        case E_POINTER:
            return invalid_argument;
        case E_NOTIMPL:
        case E_NOINTERFACE:
        case REGDB_E_CLASSNOTREG:
        case CLASS_E_CLASSNOTAVAILABLE:
        case not_supported_hr:
            return not_supported;
        case E_OUTOFMEMORY:
            return out_of_memory;
        case not_found_hr:
            return not_found;
        case timeout_hr:
        case sem_timeout_hr:
            return timeout;
        case not_connected_hr:
        case closed_hr:
            return closed;
        default:
            return io_error;
    }
}

bool parse_hex(std::string_view text, uint64_t & value)
{
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    return ec == std::errc() && end == text.data() + text.size();
}

} // namespace

Apartment::Apartment(): _hr(RoInitialize(RO_INIT_MULTITHREADED)) {}

Apartment::~Apartment()
{
    if (SUCCEEDED(_hr))
        RoUninitialize();
}

Result<void> Apartment::entered() const
{
    // a thread already in a single threaded apartment still reaches the agile bluetooth objects
    if (FAILED(_hr) && _hr != RPC_E_CHANGED_MODE)
        return std::unexpected(error_of(_hr, "windows runtime initialization"));
    return {};
}

Error error_of(HRESULT hr, std::string_view what)
{
    return Error(code_of(hr),
                 "{}: 0x{:08x} {}",
                 what,
                 static_cast<ULONG>(hr),
                 sys::os::error_str(static_cast<int>(hr)));
}

std::string to_utf8(HSTRING hstring)
{
    UINT32 length = 0;
    const wchar_t *buffer = WindowsGetStringRawBuffer(hstring, &length);
    return str::to_str(std::wstring_view(buffer, length));
}

Microsoft::WRL::Wrappers::HString hstring_of(std::string_view text)
{
    const std::wstring wide = str::to_wstr(text);
    Microsoft::WRL::Wrappers::HString hstring;
    hstring.Set(wide.c_str(), static_cast<unsigned int>(wide.size()));
    return hstring;
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

Result<GUID> guid_of(const std::string & uuid)
{
    // 8-4-4-4-12 hexadecimal digits
    constexpr std::array<size_t, 4> dashes = {8, 13, 18, 23};
    uint64_t data1 = 0;
    uint64_t data2 = 0;
    uint64_t data3 = 0;
    uint64_t data4_high = 0;
    uint64_t data4_low = 0;
    const std::string_view text(uuid);
    const bool valid = text.size() == 36 && std::ranges::all_of(dashes, [&text](size_t i) { return text[i] == '-'; })
                       && parse_hex(text.substr(0, 8), data1) && parse_hex(text.substr(9, 4), data2)
                       && parse_hex(text.substr(14, 4), data3) && parse_hex(text.substr(19, 4), data4_high)
                       && parse_hex(text.substr(24, 12), data4_low);
    if (!valid)
        return std::unexpected(Error(invalid_argument, "not a 128 bits uuid: '{}'", uuid));

    GUID guid {};
    guid.Data1 = static_cast<unsigned long>(data1);
    guid.Data2 = static_cast<unsigned short>(data2);
    guid.Data3 = static_cast<unsigned short>(data3);
    guid.Data4[0] = static_cast<unsigned char>(data4_high >> 8);
    guid.Data4[1] = static_cast<unsigned char>(data4_high);
    for (size_t i = 0; i < 6; ++i)
        guid.Data4[2 + i] = static_cast<unsigned char>(data4_low >> (8 * (5 - i)));
    return guid;
}

std::vector<uint8_t> read_buffer(streams::IBuffer *buffer)
{
    ComPtr<::Windows::Storage::Streams::IBufferByteAccess> byte_access;
    UINT32 length = 0;
    byte *data = nullptr;
    if (buffer == nullptr || FAILED(buffer->QueryInterface(IID_PPV_ARGS(&byte_access)))
        || FAILED(buffer->get_Length(&length)) || FAILED(byte_access->Buffer(&data)))
        return {};
    return std::vector<uint8_t>(data, data + length);
}

Result<ComPtr<streams::IBuffer>> make_buffer(ArrUByteView bytes)
{
    auto buffers = factory<streams::IBufferFactory>(RuntimeClass_Windows_Storage_Streams_Buffer);
    SIHD_UNEXPECTED_RETURN(buffers);
    ComPtr<streams::IBuffer> buffer;
    ComPtr<::Windows::Storage::Streams::IBufferByteAccess> byte_access;
    byte *data = nullptr;
    const auto size = static_cast<UINT32>(bytes.size());
    HRESULT hr = (*buffers)->Create(size, &buffer);
    if (SUCCEEDED(hr))
        hr = buffer->put_Length(size);
    if (SUCCEEDED(hr))
        hr = buffer.As(&byte_access);
    if (SUCCEEDED(hr))
        hr = byte_access->Buffer(&data);
    if (FAILED(hr))
        return std::unexpected(error_of(hr, "windows runtime buffer"));
    if (size > 0)
        std::memcpy(data, bytes.data(), size);
    return buffer;
}

Error async_error(IInspectable *operation, std::optional<AsyncStatus> status, std::string_view what)
{
    ComPtr<IAsyncInfo> info;
    const bool known = SUCCEEDED(operation->QueryInterface(IID_PPV_ARGS(&info)));
    if (!status.has_value())
    {
        if (known)
            info->Cancel();
        return Error(ErrorCode::timeout, "{}: no answer", what);
    }
    if (*status == AsyncStatus::Canceled)
        return Error(interrupted, "{}: canceled", what);
    HRESULT hr = E_FAIL;
    if (known)
        info->get_ErrorCode(&hr);
    return error_of(hr, what);
}

Error bluetooth_error(bluetooth::BluetoothError error, std::string_view what)
{
    switch (error)
    {
        case bluetooth::BluetoothError_RadioNotAvailable:
            return Error(not_initialized, "{}: the bluetooth radio is off", what);
        case bluetooth::BluetoothError_ResourceInUse:
            return Error(would_block, "{}: the bluetooth resource is in use", what);
        case bluetooth::BluetoothError_DeviceNotConnected:
            return Error(closed, "{}: the bluetooth device is not connected", what);
        case bluetooth::BluetoothError_DisabledByPolicy:
        case bluetooth::BluetoothError_DisabledByUser:
        case bluetooth::BluetoothError_ConsentRequired:
            return Error(permission_denied, "{}: bluetooth access refused", what);
        case bluetooth::BluetoothError_NotSupported:
        case bluetooth::BluetoothError_TransportNotSupported:
            return Error(not_supported, "{}: not supported by the bluetooth radio", what);
        default:
            return Error(io_error, "{}: bluetooth error {}", what, static_cast<int>(error));
    }
}

Result<void> gatt_status(gatt::GattCommunicationStatus status,
                         foundation::IReference<BYTE> *protocol_error,
                         std::string_view what)
{
    switch (status)
    {
        case gatt::GattCommunicationStatus_Success:
            return {};
        case gatt::GattCommunicationStatus_Unreachable:
            return std::unexpected(Error(closed, "{}: the bluetooth device is unreachable", what));
        case gatt::GattCommunicationStatus_AccessDenied:
            return std::unexpected(Error(permission_denied, "{}: access denied", what));
        default:
            break;
    }
    BYTE att_status = 0;
    if (protocol_error == nullptr || FAILED(protocol_error->get_Value(&att_status)))
        return std::unexpected(Error(io_error, "{}: gatt protocol error", what));
    return std::unexpected(att::error_of(att_status, what));
}

} // namespace sihd::bt::winrt
