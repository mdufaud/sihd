#include <fmt/format.h>
#include <fmt/ranges.h>

#include <sihd/sys/App.hpp>
#include <sihd/usb/UsbContext.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#if defined(__SIHD_LINUX__)
# include <sihd/usb/linux/hwdb.hpp>
#endif

using namespace sihd::util;
using namespace sihd::usb;

SIHD_NEW_LOGGER("demo");

namespace demo
{

std::string speed_str(UsbSpeed speed)
{
    switch (speed)
    {
        case UsbSpeed::low:
            return "1.5M";
        case UsbSpeed::full:
            return "12M";
        case UsbSpeed::high:
            return "480M";
        case UsbSpeed::super:
            return "5G";
        case UsbSpeed::super_plus:
            return "10G";
        default:
            return "?";
    }
}

std::string describe(const UsbDevice & device)
{
    return fmt::format(
        "bus {:03} device {:03} port [{}] id {:04x}:{:04x} usb {:x}.{:02x} {:>4} class {:02x}/{:02x}/{:02x} "
        "'{}' '{}' ({} / {} / {})",
        device.bus,
        device.address,
        fmt::join(device.port_path, "."),
        device.vendor_id,
        device.product_id,
        device.usb_version_bcd >> 8,
        device.usb_version_bcd & 0xff,
        speed_str(device.speed),
        device.device_class,
        device.device_subclass,
        device.device_protocol,
        device.vendor_name,
        device.product_name,
        device.class_name,
        device.subclass_name,
        device.protocol_name);
}

} // namespace demo

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "usb_scan_demo",
        .description = "USB device scan demo of module usb",
    });

    app.root().on_run([&] {
        UsbContext context;
        auto devices = context.scan();
        if (SIHD_UNEXPECTED_LOG(devices))
        {
            app.exit(EXIT_FAILURE);
            return;
        }

#if defined(__SIHD_LINUX__)
        auto named = linux_::hwdb::enrich(*devices);
        if (!named.has_value())
            SIHD_LOG(warning, "no usb names: {}", named.error().message);
#endif

        SIHD_COUT("{} devices:\n", devices->size());
        for (const UsbDevice & device : *devices)
            SIHD_COUT("  {}\n", demo::describe(device));
    });

    return app.run(argc, argv);
}
