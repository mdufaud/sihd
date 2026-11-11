#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <fmt/format.h>

#include <sihd/sys/App.hpp>
#include <sihd/usb/UsbHandle.hpp>

using namespace sihd::util;
using namespace sihd::usb;

namespace
{

// GET_DESCRIPTOR: standard device-to-host request 6, descriptor type DEVICE (1) in the high byte of value
constexpr UsbHandle::ControlSetup get_device_descriptor {
    .request_type = 0x80,
    .request = 6,
    .value = 0x0100,
    .index = 0,
};
// bLength of a device descriptor
constexpr size_t device_descriptor_size = 18;

std::string text_of(const Result<std::string> & text)
{
    return text.has_value() ? *text : fmt::format("({})", text.error().message);
}

void print_configurations(const UsbDevice & device)
{
    for (const UsbConfiguration & configuration : device.configurations)
    {
        fmt::print("configuration {}: {} mA\n", configuration.value, configuration.max_power_ma);
        for (const UsbInterface & usb_interface : configuration.interfaces)
        {
            for (const UsbAltSetting & setting : usb_interface.alt_settings)
            {
                fmt::print("  interface {} alt {}: class {:02x}:{:02x}:{:02x}\n",
                           usb_interface.number,
                           setting.number,
                           setting.interface_class,
                           setting.interface_subclass,
                           setting.interface_protocol);
                for (const UsbEndpoint & endpoint : setting.endpoints)
                    fmt::print("    endpoint {:02x}: {} bytes\n", endpoint.address, endpoint.max_packet_size);
            }
        }
    }
}

void describe(sihd::sys::App & app, int usb_fd)
{
    UsbHandle handle;
    auto wrapped = handle.wrap(usb_fd);
    if (!wrapped)
    {
        fmt::print(stderr, "{}\n", wrapped.error().message);
        app.exit(EXIT_FAILURE);
        return;
    }
    auto device = handle.device();
    if (!device)
    {
        fmt::print(stderr, "{}\n", device.error().message);
        app.exit(EXIT_FAILURE);
        return;
    }

    fmt::print("device {:04x}:{:04x} usb {:x}.{:02x}\n",
               device->vendor_id,
               device->product_id,
               device->usb_version_bcd >> 8,
               device->usb_version_bcd & 0xff);
    fmt::print("manufacturer: {}\n", text_of(handle.manufacturer()));
    fmt::print("product: {}\n", text_of(handle.product()));
    fmt::print("serial number: {}\n", text_of(handle.serial_number()));
    print_configurations(*device);

    std::array<uint8_t, device_descriptor_size> descriptor {};
    auto size = handle.control_read(get_device_descriptor,
                                    descriptor.data(),
                                    descriptor.size(),
                                    std::chrono::seconds(1));
    if (!size)
    {
        fmt::print(stderr, "{}\n", size.error().message);
        app.exit(EXIT_FAILURE);
        return;
    }
    fmt::print("device descriptor read by control transfer: {} bytes\n", *size);
}

} // namespace

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "usb_control",
        .description = "Print the descriptors of the usb device an android app opened through UsbManager",
    });

    int usb_fd = -1;
    app.root().bind("usb-fd", usb_fd, "descriptor of UsbManager.openDevice(device).getFileDescriptor()");
    app.root().on_run([&] {
        if (usb_fd < 0)
        {
            fmt::print(stderr, "--usb-fd is required: plug a device through OTG and start the app again\n");
            app.exit(EXIT_FAILURE);
            return;
        }
        describe(app, usb_fd);
    });

    return app.run(argc, argv);
}
