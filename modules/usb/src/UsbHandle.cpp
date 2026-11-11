#include <array>
#include <cstdint>
#include <limits>
#include <string>

#include <fmt/format.h>

#include <sihd/usb/UsbHandle.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/build.hpp>

#include "libusb.hpp"

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::usb
{

namespace
{

// a usb string descriptor holds at most 126 utf-16 characters
constexpr size_t string_size_max = 256;
// libusb tracks the claimed interfaces in a 32 bit mask and refuses higher numbers
constexpr int interface_count_max = 32;

// libusb never writes into the buffer of an out transfer
unsigned char *writable_bytes(ArrByteView data)
{
    return reinterpret_cast<unsigned char *>(const_cast<int8_t *>(data.data()));
}

using TransferFunction =
    int(LIBUSB_CALL *)(libusb_device_handle *, unsigned char, unsigned char *, int, int *, unsigned int);

Result<size_t> sync_transfer(TransferFunction function,
                             libusb_device_handle *handle,
                             uint8_t endpoint,
                             bool in,
                             unsigned char *data,
                             size_t size,
                             Duration timeout,
                             std::string_view operation)
{
    if (((endpoint & LIBUSB_ENDPOINT_IN) != 0) != in)
    {
        return std::unexpected(Error(invalid_argument,
                                     "usb {} needs an {} endpoint, got 0x{:02x}",
                                     operation,
                                     in ? "in" : "out",
                                     endpoint));
    }
    if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
        return std::unexpected(Error(invalid_argument, "usb {} too big: {} bytes", operation, size));
    auto milliseconds = libusb::to_timeout(timeout);
    SIHD_UNEXPECTED_RETURN(milliseconds);

    int transferred = 0;
    const int rc = function(handle, endpoint, data, static_cast<int>(size), &transferred, *milliseconds);
    if (rc == LIBUSB_ERROR_TIMEOUT && transferred > 0)
        return static_cast<size_t>(transferred);
    if (rc != LIBUSB_SUCCESS)
        return std::unexpected(libusb::to_error(rc, fmt::format("complete usb {}", operation)));
    return static_cast<size_t>(transferred);
}

Result<std::string> read_string(libusb_device_handle *handle, uint8_t index, std::string_view name)
{
    if (index == 0)
        return std::unexpected(Error(not_found, "usb device has no {} string", name));

    std::array<unsigned char, string_size_max> buffer {};
    const int rc = libusb_get_string_descriptor_ascii(handle, index, buffer.data(), static_cast<int>(buffer.size()));
    if (rc < 0)
        return std::unexpected(libusb::to_error(rc, fmt::format("read usb {} string", name)));
    return std::string(reinterpret_cast<const char *>(buffer.data()), static_cast<size_t>(rc));
}

Result<size_t> control_transfer(libusb_device_handle *handle,
                                const UsbHandle::ControlSetup & setup,
                                bool in,
                                unsigned char *data,
                                size_t size,
                                Duration timeout,
                                std::string_view operation)
{
    if (((setup.request_type & LIBUSB_ENDPOINT_IN) != 0) != in)
    {
        return std::unexpected(Error(invalid_argument,
                                     "usb {} needs an {} request type, got 0x{:02x}",
                                     operation,
                                     in ? "in" : "out",
                                     setup.request_type));
    }
    if (size > std::numeric_limits<uint16_t>::max())
        return std::unexpected(Error(invalid_argument, "usb {} too big: {} bytes", operation, size));
    auto milliseconds = libusb::to_timeout(timeout);
    SIHD_UNEXPECTED_RETURN(milliseconds);

    const int rc = libusb_control_transfer(handle,
                                           setup.request_type,
                                           setup.request,
                                           setup.value,
                                           setup.index,
                                           data,
                                           static_cast<uint16_t>(size),
                                           *milliseconds);
    if (rc < 0)
        return std::unexpected(libusb::to_error(rc, fmt::format("complete usb {}", operation)));
    return static_cast<size_t>(rc);
}

Result<void> check_result(int rc, std::string_view operation)
{
    if (rc != LIBUSB_SUCCESS)
        return std::unexpected(libusb::to_error(rc, operation));
    return {};
}

} // namespace

struct UsbHandle::Impl
{
        libusb::Context context;
        libusb_device_handle *handle = nullptr;

        Impl(libusb::Context context, libusb_device_handle *handle): context(std::move(context)), handle(handle) {}

        ~Impl()
        {
            // libusb_close releases nothing: a wrapped fd keeps its claims, an auto-detached kernel driver stays detached
            for (int number = 0; number < interface_count_max; ++number)
                libusb_release_interface(handle, number);
            libusb_close(handle);
        }

        Impl(const Impl &) = delete;
        Impl & operator=(const Impl &) = delete;

        static Result<libusb_device_handle *> checked(const std::unique_ptr<Impl> & impl)
        {
            if (impl == nullptr)
                return std::unexpected(Error(not_initialized, "no usb handle"));
            return impl->handle;
        }
};

UsbHandle::UsbHandle() = default;

UsbHandle::~UsbHandle() = default;

UsbHandle::UsbHandle(UsbHandle && other) noexcept = default;

UsbHandle & UsbHandle::operator=(UsbHandle && other) noexcept = default;

Result<void> UsbHandle::open(const UsbDevice & device)
{
    if (_impl != nullptr)
        return std::unexpected(Error(already_exists, "the usb handle is already open"));
    if constexpr (build::is_android)
        return std::unexpected(Error(not_supported, "cannot open a listed usb device on android"));
    auto usb_context = libusb::init_context();
    SIHD_UNEXPECTED_RETURN(usb_context);

    libusb_device **list = nullptr;
    const ssize_t count = libusb_get_device_list(usb_context->get(), &list);
    if (count < 0)
        return std::unexpected(libusb::to_error(static_cast<int>(count), "list usb devices"));
    Defer free_list([list] { libusb_free_device_list(list, 1); });

    for (ssize_t i = 0; i < count; ++i)
    {
        const libusb_device_descriptor descriptor = libusb::descriptor_of(list[i]);
        // a replugged device may get the address of the one that left
        const bool same_device = libusb_get_bus_number(list[i]) == device.bus
                                 && libusb_get_device_address(list[i]) == device.address
                                 && descriptor.idVendor == device.vendor_id
                                 && descriptor.idProduct == device.product_id;
        if (!same_device)
            continue;
        libusb_device_handle *handle = nullptr;
        const int rc = libusb_open(list[i], &handle);
        if (rc != LIBUSB_SUCCESS)
            return std::unexpected(libusb::to_error(rc, "open usb device"));
        _impl = std::make_unique<Impl>(std::move(*usb_context), handle);
        return {};
    }
    return std::unexpected(Error(not_found,
                                 "cannot open usb device: no device {:04x}:{:04x} at {:03}:{:03}",
                                 device.vendor_id,
                                 device.product_id,
                                 device.bus,
                                 device.address));
}

Result<void> UsbHandle::wrap(int fd)
{
    if (_impl != nullptr)
        return std::unexpected(Error(already_exists, "the usb handle is already open"));
    auto usb_context = libusb::init_context();
    SIHD_UNEXPECTED_RETURN(usb_context);

    libusb_device_handle *handle = nullptr;
    const int rc = libusb_wrap_sys_device(usb_context->get(), static_cast<intptr_t>(fd), &handle);
    if (rc != LIBUSB_SUCCESS)
        return std::unexpected(libusb::to_error(rc, "wrap usb device"));
    _impl = std::make_unique<Impl>(std::move(*usb_context), handle);
    return {};
}

void UsbHandle::close()
{
    _impl.reset();
}

bool UsbHandle::is_open() const
{
    return _impl != nullptr;
}

Result<UsbDevice> UsbHandle::device() const
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return libusb::to_device(libusb_get_device(*handle));
}

Result<std::string> UsbHandle::manufacturer() const
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return read_string(*handle, libusb::descriptor_of(libusb_get_device(*handle)).iManufacturer, "manufacturer");
}

Result<std::string> UsbHandle::product() const
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return read_string(*handle, libusb::descriptor_of(libusb_get_device(*handle)).iProduct, "product");
}

Result<std::string> UsbHandle::serial_number() const
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return read_string(*handle, libusb::descriptor_of(libusb_get_device(*handle)).iSerialNumber, "serial number");
}

Result<uint8_t> UsbHandle::configuration() const
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    int value = 0;
    const int rc = libusb_get_configuration(*handle, &value);
    if (rc != LIBUSB_SUCCESS)
        return std::unexpected(libusb::to_error(rc, "get usb configuration"));
    return static_cast<uint8_t>(value);
}

Result<void> UsbHandle::set_configuration(uint8_t value)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return check_result(libusb_set_configuration(*handle, value), "set usb configuration");
}

Result<void> UsbHandle::claim_interface(uint8_t interface_number)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return check_result(libusb_claim_interface(*handle, interface_number), "claim usb interface");
}

Result<void> UsbHandle::release_interface(uint8_t interface_number)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return check_result(libusb_release_interface(*handle, interface_number), "release usb interface");
}

Result<void> UsbHandle::set_alt_setting(uint8_t interface_number, uint8_t alt_setting)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return check_result(libusb_set_interface_alt_setting(*handle, interface_number, alt_setting),
                        "set usb alt setting");
}

Result<void> UsbHandle::set_auto_detach_kernel_driver(bool active)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return check_result(libusb_set_auto_detach_kernel_driver(*handle, active ? 1 : 0),
                        "set usb kernel driver auto detach");
}

Result<void> UsbHandle::clear_halt(uint8_t endpoint)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return check_result(libusb_clear_halt(*handle, endpoint), "clear usb endpoint halt");
}

Result<void> UsbHandle::reset()
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return check_result(libusb_reset_device(*handle), "reset usb device");
}

Result<size_t> UsbHandle::control_read(const ControlSetup & setup, void *data, size_t size, Duration timeout)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return control_transfer(*handle, setup, true, static_cast<unsigned char *>(data), size, timeout, "control read");
}

Result<size_t> UsbHandle::control_write(const ControlSetup & setup, ArrByteView data, Duration timeout)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return control_transfer(*handle, setup, false, writable_bytes(data), data.size(), timeout, "control write");
}

Result<size_t> UsbHandle::bulk_read(uint8_t endpoint, void *data, size_t size, Duration timeout)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return sync_transfer(libusb_bulk_transfer,
                         *handle,
                         endpoint,
                         true,
                         static_cast<unsigned char *>(data),
                         size,
                         timeout,
                         "bulk read");
}

Result<size_t> UsbHandle::bulk_write(uint8_t endpoint, ArrByteView data, Duration timeout)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return sync_transfer(libusb_bulk_transfer,
                         *handle,
                         endpoint,
                         false,
                         writable_bytes(data),
                         data.size(),
                         timeout,
                         "bulk write");
}

Result<size_t> UsbHandle::interrupt_read(uint8_t endpoint, void *data, size_t size, Duration timeout)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return sync_transfer(libusb_interrupt_transfer,
                         *handle,
                         endpoint,
                         true,
                         static_cast<unsigned char *>(data),
                         size,
                         timeout,
                         "interrupt read");
}

Result<size_t> UsbHandle::interrupt_write(uint8_t endpoint, ArrByteView data, Duration timeout)
{
    auto handle = Impl::checked(_impl);
    SIHD_UNEXPECTED_RETURN(handle);
    return sync_transfer(libusb_interrupt_transfer,
                         *handle,
                         endpoint,
                         false,
                         writable_bytes(data),
                         data.size(),
                         timeout,
                         "interrupt write");
}

} // namespace sihd::usb
