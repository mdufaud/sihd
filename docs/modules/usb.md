# usb

USB device enumeration through libusb. A context lists the connected devices with their device descriptor and bus topology, without opening them. On Linux, the udev hardware database adds vendor, product and class names.

| | |
|---|---|
| Depends on | `sys` |
| External libraries | libusb |
| Platforms | Linux, Windows |
| Build | `make m=usb` |
| Header | `#include <sihd/usb.hpp>` |

## Capabilities

### Enumeration

- `UsbContext`: owns a private libusb context, move-only. `scan()` returns a `UsbDevice` per connected device. When libusb cannot initialize (no usbfs, for example in a container) or the context was moved from, `scan()` returns a `not_initialized` error.
- `UsbDevice`: `bus`, `address`, `port_path` (empty for a root hub), `speed` (`UsbSpeed`), `usb_version_bcd`, `vendor_id`, `product_id`, `release_bcd`, `device_class`, `device_subclass`, `device_protocol`, `max_packet_size0`, `configuration_count`.
- Descriptor strings (manufacturer, product, serial) need an opened device and are not read.

### Hardware database names (Linux)

- `linux_::hwdb::enrich(devices)` in `<sihd/usb/linux/hwdb.hpp>` fills `vendor_name`, `product_name`, `class_name`, `subclass_name` and `protocol_name`. Entries the database does not know stay empty.
- Reads the systemd `hwdb.bin` file directly (`/etc/systemd/hwdb`, `/etc/udev`, `/usr/lib/systemd/hwdb`, `/usr/lib/udev`, first found), without libudev: it works the same on native and cross-compiled Linux builds. Returns `not_found` when the system has no `hwdb.bin` and `io_error` when the file is not a valid database.
- Linux only: the header does not compile on other platforms. On Windows the name fields stay empty.

## Example

```cpp
#include <sihd/usb.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>
#if defined(__SIHD_LINUX__)
# include <sihd/usb/linux/hwdb.hpp>
#endif

using namespace sihd::usb;

SIHD_NEW_LOGGER("demo");

void list_devices()
{
    UsbContext context;
    auto devices = context.scan();
    if (!devices.has_value())
    {
        SIHD_LOG(error, "{}", devices.error().message);
        return;
    }

#if defined(__SIHD_LINUX__)
    if (auto named = linux_::hwdb::enrich(*devices); !named.has_value())
        SIHD_LOG(warning, "no usb names: {}", named.error().message);
#endif

    for (const UsbDevice & device : *devices)
    {
        SIHD_LOG(info,
                 "bus {:03} device {:03}: {:04x}:{:04x} {} {}",
                 device.bus,
                 device.address,
                 device.vendor_id,
                 device.product_id,
                 device.vendor_name,
                 device.product_name);
    }
}
```

Adapted from `modules/usb/demo/usb_scan_demo.cpp` (`make demo m=usb`).

## Scripting

Not exposed to Lua or Python.
