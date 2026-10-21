# bt

Bluetooth Low Energy scanning. On Linux, it discovers devices through BlueZ over D-Bus. On Windows, it uses the WinRT advertisement watcher. A scanner reports each discovered device to observers as its advertisement is seen, and keeps the latest result per address.

| | |
|---|---|
| Depends on | `util`, `sys` |
| External libraries | none (Linux: D-Bus through `sys`; Windows: WinRT runtime) |
| Platforms | Linux, Windows |
| Build | `make m=bt` |
| Header | `#include <sihd/bt/Scanner.hpp>` |

## Capabilities

### Scanning

- `Scanner`: BLE discovery service and observable. `set_adapter(index)` selects the adapter, `scan_for(duration)` scans for a fixed time, `start` and `stop` scan until stopped. Observers are notified each time a device is new or changed.
- `Scanner::results`: map of address to `ScanResult`, valid once the scan stopped or from an observer. `last_result` returns the most recent one.
- `ScanResult`: device data. `identifier` (name), `address`, `address_type`, `rssi`, `tx_power`, `manufacturer_data`, `service_data`, `uuids`, `timestamp`. `connectable` is always false on Linux.

### Adapters

- `bluetooth_enabled`: whether the Bluetooth adapter is powered.
- `list_adapters`: returns `AdapterInfo` (`identifier`, `address`) for each adapter.
- Linux only. On Windows both functions return a `not_supported` error.

## Example

```cpp
#include <sihd/bt/Scanner.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;
using namespace sihd::bt;

SIHD_NEW_LOGGER("demo");

void scan_nearby()
{
    Scanner scanner("scanner");
    Handler<Scanner *> printer([](Scanner *obj) {
        if (auto result = obj->last_result(); result.has_value())
            SIHD_LOG(info, "{} {} dBm", result->address, result->rssi);
    });
    scanner.add_observer(&printer);

    if (!scanner.set_adapter(0))
        return;
    scanner.scan_for(Duration(time::sec(10)));

    for (const auto & [address, result] : scanner.results())
        SIHD_LOG(info, "{}: {}", address, result.identifier);
}
```

Adapted from `modules/bt/demo/bt_scan_demo.cpp`.

## Scripting

Not exposed to Lua or Python.
