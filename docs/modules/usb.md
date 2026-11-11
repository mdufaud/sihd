# usb

USB devices through libusb: list the connected devices with their full descriptors, open one (from the list or from a file descriptor an Android app got from `UsbManager`), run control, bulk and interrupt transfers, and follow devices being plugged and unplugged. On Linux, the udev hardware database adds vendor, product and class names.

| | |
|---|---|
| Depends on | `sys` |
| External libraries | libusb |
| Platforms | Linux, Windows, Android |
| Build | `make m=usb` |
| Header | `#include <sihd/usb.hpp>` |

## Capabilities

### Enumeration

- `scan()` returns a `UsbDevice` per connected device, without opening them. When libusb cannot initialize (no usbfs, for example in a container), `scan()` returns a `not_initialized` error. On Android `scan()` returns `not_supported`: an app cannot list `/dev/bus/usb`.
- `UsbDevice`: `bus`, `address`, `port_path` (empty for a root hub), `speed` (`UsbSpeed`), `usb_version_bcd`, `vendor_id`, `product_id`, `release_bcd`, `device_class`, `device_subclass`, `device_protocol`, `max_packet_size0`, `configuration_count`, `configurations`.
- `UsbConfiguration`: `value`, `attributes`, `max_power_ma` (bMaxPower in 8 mA units at SuperSpeed and above, 2 mA below), `interfaces`. `UsbInterface`: `number`, `alt_settings`. `UsbAltSetting`: `number`, `interface_class`, `interface_subclass`, `interface_protocol`, `endpoints`. `UsbEndpoint`: `address` (bit 7 set for in), `direction` (`UsbDirection`), `transfer_type` (`UsbTransferType`: control, isochronous, bulk, interrupt), `max_packet_size` (bits 10:0 of wMaxPacketSize: the extra transactions per microframe of a high-speed isochronous or interrupt endpoint are left out), `interval`.
- A configuration the system refuses to read is left out: `configurations.size()` is then smaller than `configuration_count`, the rest of the scan is kept. Class-specific descriptors (HID report, CDC functional) are not decoded.

### Opened device

- `UsbHandle`, move-only, starts closed. `is_open()`, `close()`; opening an open handle gives `already_exists` and keeps it open.
  - `open(device)` reopens a scanned device by bus and address: `not_found` once it was unplugged, got a new address or another device took its address (bus, address, vendor and product must match), `permission_denied` when the user cannot write its `/dev/bus/usb` node (Linux). Windows opens only a device bound to a WinUSB-compatible driver (WinUSB, libusbK). `not_supported` on Android.
  - `wrap(fd)` adopts an opened usbfs descriptor, the only way on Android (`UsbManager.openDevice(device).getFileDescriptor()`); it also works on Linux with a `/dev/bus/usb/BBB/DDD` node opened read-write. The caller keeps owning `fd` and closes it after the handle. An invalid descriptor gives `io_error`, Windows gives `not_supported`.
- `device()` returns the `UsbDevice` with its descriptors, useful after `wrap`. `manufacturer()`, `product()`, `serial_number()` read the string descriptors in ASCII (other characters become `?`), `not_found` when the device declares none.
- `configuration()`, `set_configuration(value)`, `claim_interface(interface_number)`, `release_interface(interface_number)`, `set_alt_setting(interface_number, alt_setting)`, `clear_halt(endpoint)`, `reset()` (the device may enumerate again: the handle then answers `not_found`), `set_auto_detach_kernel_driver(active)` (Linux and Android only, `not_supported` on Windows). Closing the handle releases its claimed interfaces, after `wrap` too, and gives back the kernel drivers the auto detach took.

### Transfers

- Synchronous, one function per direction, returning the transferred size: `control_read(setup, data, size, timeout)` / `control_write(setup, view, timeout)` with a `UsbHandle::ControlSetup { request_type, request, value, index }`; `bulk_read` / `bulk_write`, `interrupt_read` / `interrupt_write` on an endpoint address. Reads take a buffer pointer and its size, writes an `ArrByteView`.
- `invalid_argument` without touching the device when the direction disagrees with the call (bit 7 of `request_type` or of the endpoint), when a control transfer exceeds 65535 bytes, or when the timeout is zero.
- Timeout (`Duration`): negative waits forever, zero is refused (libusb reads 0 as forever), below a millisecond counts as one. A timeout after part of a bulk or interrupt transfer returns the transferred size, `timeout` only when nothing moved.

| Failure | Code (Linux, Windows, Android) |
|---|---|
| libusb cannot initialize, closed or moved-from handle | `not_initialized` |
| handle already open | `already_exists` |
| device unplugged, readdressed, or no such string | `not_found` |
| node not writable | `permission_denied` |
| endpoint stall, busy interface, I/O error | `io_error` |
| transfer timeout with nothing transferred | `timeout` |
| device sent more than the buffer | `overflow` |
| feature absent on the platform | `not_supported` |

### Hotplug

- `UsbMonitor` (`Observable<UsbMonitor>` + `IRunnable`, form of `NetMonitor`): each `run()` handles libusb events for up to `set_run_timeout` (100 ms by default, negative waits for a change), fills `events()` (`UsbMonitorEvent { type, device }`, `UsbMonitorEventType::arrived` or `left`) and notifies the observers when there are some. The devices already plugged arrive at the first `run()`, which then returns without waiting its timeout, so no device is missed between a scan and the monitor.
- `UsbMonitor::supported` is true on Linux only. The constructor opens the subscription through a private `Result`-returning open and throws its error message: on Windows (libusb has no hotplug there), on Android (no device discovery) and when the subscription is refused. libusb is built without udev here: hotplug uses its netlink backend.

### Hardware database names (Linux)

- `linux_::hwdb::enrich(devices)` in `<sihd/usb/linux/hwdb.hpp>` fills `vendor_name`, `product_name`, `class_name`, `subclass_name` and `protocol_name`. Entries the database does not know stay empty.
- Reads the systemd `hwdb.bin` file directly (`/etc/systemd/hwdb`, `/etc/udev`, `/usr/lib/systemd/hwdb`, `/usr/lib/udev`, first found), without libudev: it works the same on native and cross-compiled Linux builds. Returns `not_found` when the system has no `hwdb.bin` and `io_error` when the file is not a valid database.
- Linux only: the header does not compile on other platforms. On Windows the name fields stay empty.

### Android

- The demo app (`make m=usb demo=1 platform=android`, sources in `modules/usb/android/`) asks `UsbManager` for the permission on the first device plugged through OTG, opens it and starts `usb_control --usb-fd N`, which wraps the descriptor and prints the descriptors, the strings and a control read of the device descriptor. The app keeps the `UsbDeviceConnection` open while the program runs.

## Examples

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
    auto devices = scan();
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

```cpp
#include <array>
#include <chrono>

#include <sihd/usb.hpp>

using namespace sihd::usb;

// GET_DESCRIPTOR: standard device-to-host request 6, descriptor type DEVICE (1) in the high byte of value, needs no claimed interface
sihd::util::Result<size_t> read_device_descriptor(const UsbDevice & device)
{
    UsbHandle handle;
    auto opened = handle.open(device);
    if (!opened)
        return std::unexpected(std::move(opened).error());

    // bLength of a device descriptor
    std::array<uint8_t, 18> descriptor {};
    const UsbHandle::ControlSetup setup {.request_type = 0x80, .request = 6, .value = 0x0100, .index = 0};
    return handle.control_read(setup, descriptor.data(), descriptor.size(), std::chrono::seconds(1));
}
```

Adapted from `TestUsbHandleLinux.test_control_read_device_descriptor`.

## Testing

On Linux, `make itest m=usb` runs the usb test binary under `umockdev-wrapper` (`TEST_WRAPPER_MODULE_usb`, from `site_scons/addon/test.py`; native builds without sanitizer only). `test/linux/FakeUsbDevice` then serves a fake boot keyboard (`1209:0001` at `001:002`, with a read-only twin at `001:003`) through a umockdev testbed: `TestUsbHandleLinux` opens, reads its strings, runs control and interrupt transfers, a stalled endpoint and a partial timeout against it, `TestUsbDeviceLinux` checks its parsed descriptors. Without `umockdev` (`pacman -S umockdev`, `apt install libumockdev-dev umockdev`), or with a sanitizer, the same tests use the real devices whose `/dev/bus/usb` node is writable and skip otherwise.

Limits of the fake: libusb, built without udev, ignores umockdev's uevents, so `UsbMonitor` would see the fake devices arrive but never leave and its tests keep the real devices; a wrapped descriptor has no bus number (umockdev serves the node as a socket); `TestUsbHandleLinux.test_wrap_invalid_fd` skips, umockdev blocking on `ioctl(-1)`.

## Scripting

Not exposed to Lua or Python.
