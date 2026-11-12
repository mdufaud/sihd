# bt

Bluetooth Classic and Low Energy with one API on Linux, Windows and Android. It lists and sets adapters, scans, connects to a device and explores, reads, writes and subscribes to its GATT attributes, pairs, opens RFCOMM and L2CAP LE byte streams and accepts them, and plays the peripheral role with a GATT server and an advertiser. Each platform goes through its own stack: BlueZ over D-Bus and kernel sockets on Linux, WinRT and Winsock on Windows, `android.bluetooth` through a Java shim on Android. What a platform refuses returns a `not_supported` error there: the library never emulates it another way.

| | |
|---|---|
| Depends on | `sys`, `net` (RFCOMM and L2CAP go through `net::Socket`; `net` pulls `crypto` and OpenSSL) |
| External libraries | none (Linux: D-Bus through `sys`; Windows: `runtimeobject`, `bthprops`, `ws2_32`; Android: the Java shim of `modules/bt/java`, shipped into the APK) |
| Platforms | Linux, Windows, Android |
| Build | `make m=bt` |
| Headers | `#include <sihd/bt/Scanner.hpp>`, `Device.hpp`, `Socket.hpp`, `GattServer.hpp`, `Advertiser.hpp`, `adapter.hpp`, `Gatt.hpp` |

Addresses are upper case `AA:BB:CC:DD:EE:FF` strings, uuids 128 bits lower case strings, bytes `std::vector<uint8_t>` out and `ArrUByteView` in.

## Capabilities

### Adapters

- `list_adapters`: `AdapterInfo` for each adapter: `identifier`, `address`, `name`, `alias`, `powered`, `discoverable`, `pairable`, `discovering`.
- `bluetooth_enabled`: whether one adapter is powered.
- `set_adapter_powered`, `set_adapter_discoverable`, `set_adapter_pairable`, `set_adapter_alias`: by index in `list_adapters` order, `not_found` past the last one.

### Scanning

- `Scanner`: discovery service and observable. `set_adapter(index)`, `set_filter(ScanFilter)` and `set_lost_timeout(duration)` before a scan, `scan_for(duration)` scans for a fixed time, `start` and `stop` scan until stopped. A `stop` landing as a scan ends never stops the next one. Observers run on the scanning thread each time a device is new, its advertisement changed or it is lost: `last_event` is `Scanner::Event::found`, `updated` or `lost`.
- `set_lost_timeout`: a device silent that long is `lost`, then leaves `results` once the observers returned. 0, the default, keeps every device; a negative duration is refused.
- `ScanFilter`: `transport` (`Transport::any`, `le`, `classic`), `uuids` (a device advertising one of them passes), `min_rssi`. Empty fields let everything through.
- `Scanner::results`: map of address to `ScanResult`, read from an observer or once the scan stopped; `last_result` returns the latest one.
- `ScanResult`: `identifier` (name), `address`, `address_type`, `rssi`, `tx_power`, `connectable`, `manufacturer_data`, `service_data`, `uuids`, `appearance`, `class_of_device`, `timestamp`.

### Device and GATT client

- `Device`: a remote device, `Named`, `Configurable` (`address`, `adapter`) and observable. `start` connects and blocks until `stop` or a lost link, `set_address` refuses a malformed address. Observers run on the `start` thread: `last_event` is `Device::Event::connected`, `disconnected` or `notification`, `last_notification` the `GattNotification` (`handle`, `uuid`, `value`, `timestamp`).
- `info`: `DeviceInfo` (`address`, `address_type`, `name`, `paired`, `connected`, `trusted`, `appearance`, `class_of_device`, `uuids`), connected or not.
- `services`: the GATT tree, `GattService` → `GattCharacteristic` (`properties` bits of `GattCharacteristic::Property`) → `GattDescriptor`, each with its attribute `handle`. It needs the device connected, and lists the handles the other calls accept: an attribute not listed by the last `services` is `not_found`.
- `read_characteristic`, `write_characteristic` (`WriteType::with_response` or `without_response`), `read_descriptor`, `write_descriptor`, `subscribe` (notifications when the characteristic notifies, indications otherwise, `not_supported` when it does neither), `unsubscribe`, `mtu`.
- A call on a device not connected is `closed`. A remote refusal maps its ATT error the same way everywhere: `permission_denied` (read or write not permitted, insufficient authentication, authorization or encryption), `invalid_argument` (invalid offset or value length), `not_supported` (request not supported), `io_error` otherwise.

### Pairing

- `pair`: blocks until the pairing ends. Without a handler, the system pairing agent or dialog answers. `set_pairing_handler` answers in the program instead: the handler receives a `PairingRequest` (`address`, `kind`, `passkey`, `pin`) and returns a `PairingAnswer` (`accept`, `passkey`, `pin`). Kinds: `confirm`, `confirm_passkey`, `display_passkey`, `enter_passkey`, `display_pin`, `enter_pin`. A displayed code needs no answer: its `accept` is ignored.
- `cancel_pairing`, `unpair`, `set_trusted`.
- An already paired device is `already_exists`, a refused or canceled pairing `permission_denied`.

### Sockets

- `Socket`: an RFCOMM or L2CAP LE byte stream, move only, closed once default constructed. `connect_rfcomm(address, uuid)` finds the channel in the SDP records of the device, `connect_rfcomm_channel(address, channel)`, `connect_l2cap(address, address_type, psm)` opens an L2CAP LE credit based channel; the constructor taking the same arguments connects or throws `std::runtime_error`, the one taking a psm listens on it. `send` writes all the bytes, `receive` fills the array up to its capacity and returns 0 once the peer closed, `close` wakes a `receive` blocked in another thread.
- Listening is a `Socket` too: `listen_rfcomm(uuid, name)` publishes an SDP record, `listen_l2cap(psm)` (0 lets the system pick, `psm` tells which), then `accept(timeout)` returns the `Socket` of the next device, a `timeout` error once it elapsed, `closed` once `close` ran from another thread. `is_listening` tells the role: `send` and `receive` on a listener, or `accept` on a stream, are `invalid_argument`.
- Connecting or listening on an open socket is `already_exists`.
- Sockets ask for no security level: Android uses its insecure sockets, the defaults of BlueZ and Winsock.

### GATT server

- `GattServer`: `add_service(LocalService)` before `start`, then `start` publishes the services and blocks until `stop`. `LocalService`: `uuid`, `primary`, `characteristics`. `LocalCharacteristic`: `uuid`, `properties`, `on_read` (the value of a remote read; without it a read answers the last written or notified value), `on_write` (false answers that the write is not permitted). Both run on a backend thread.
- `notify(service_uuid, characteristic_uuid, value)`: sends the value to the subscribed devices, `closed` before `start`, `invalid_argument` for a characteristic that neither notifies nor indicates.

### Advertising

- `Advertiser`: `set_advertisement(Advertisement)` then `start` advertises until `stop`. `Advertisement`: `local_name`, `service_uuids`, `manufacturer_data`, `service_data`, `connectable`.

### Device kind and names

- `sihd::bt::taxonomy` (`taxonomy.hpp`) compiles the stable values of the specifications only:
  - `kind_from_appearance(appearance)` (constexpr): the GAP appearance category, bits 15 to 6; the HID sub-categories, bits 5 to 0, give `keyboard`, `mouse` and `gamepad`.
  - `kind_from_class(class_of_device)` (constexpr): the major device class, bits 12 to 8, and the minor one, bits 7 to 2.
  - `kind_from_uuid(uuid)`: about thirty classic profiles and GATT services in the 128 bits base form (HID, heart rate, audio sink, hands-free, mesh proxy...).
  - `beacon(result)` recognizes an advertisement by its prefix, without decoding it: `Beacon::ibeacon` and `find_my` (Apple manufacturer data, types 0x02 0x15 and 0x12), `swift_pair` (Microsoft manufacturer data, beacon id 0x03), `eddystone`, `fast_pair` and `exposure_notification` (service data 0xFEAA, 0xFE2C, 0xFD6F). The Find My and Swift Pair prefixes come from reverse engineering and the nRF Connect SDK sources.
  - `kind(result)` tries the appearance, the class of device, the uuids, the service data uuids, then the beacon: `beacon` for iBeacon and Eddystone, `tag` for Find My, `phone` for Exposure Notification. Fast Pair and Swift Pair say nothing of the device.
  - `Kind`: `phone`, `computer`, `watch`, `wearable`, `keyboard`, `mouse`, `gamepad`, `hid`, `headphones`, `speaker`, `media`, `hearing_aid`, `health`, `sensor`, `tag`, `beacon`, `display`, `imaging`, `light`, `appliance`, `vehicle`, `network`, `game_console`, `toy`, `unknown`. `kind_str` and `beacon_str` name the values.
- `AssignedNumbers`: the names of the Bluetooth SIG assigned numbers, read at runtime from the user's copy of the SIG repository (`https://bitbucket.org/bluetooth-SIG/public`, folder `assigned_numbers/`). sihd does not ship them: the files are proprietary to the Bluetooth SIG and their header grants no license.
  - `load_company_identifiers(path)` (`company_identifiers/company_identifiers.yaml`), `load_member_uuids(path)` (`uuids/member_uuids.yaml`), `load_service_uuids(path)` (`uuids/service_uuids.yaml`). A file that is not a flat list of `- key: value` maps is `invalid_argument` with its line number, and keeps the table loaded before.
  - `company(id)`, `member(uuid)`, `service(uuid)`: the name of a 16 bits value.
  - `vendor(result)`: the company of the manufacturer data, the owner of an advertised member uuid, then the owner of a recognized beacon (Apple, Google, Microsoft), the last one without any file loaded.
- A public address (`AddressType::public_address`) starts with an IEEE OUI: `net::OuiRegistry::vendor(result.address)` names its maker.
- `bt_demo --assigned-numbers <assigned_numbers folder> --oui <oui.txt>` prints the kind, the beacon and the vendor of each scanned device.

## Support matrix

| | Linux | Windows | Android |
|---|---|---|---|
| `list_adapters`, `bluetooth_enabled` | yes | yes | yes, one adapter |
| `set_adapter_powered`, `set_adapter_discoverable` | yes | yes | no |
| `set_adapter_pairable` | yes | no | no |
| `set_adapter_alias` | yes | no | yes |
| scan LE / classic | yes / yes | yes / yes | yes / yes |
| `Device`: connect, `info`, GATT client | yes | yes, LE | yes, LE |
| `pair` through the system / a handler | yes / yes | yes / yes | yes / no |
| `cancel_pairing`, `unpair` | yes | yes | no |
| `set_trusted` | yes | no | no |
| RFCOMM by uuid, client and listener | yes | yes | yes |
| RFCOMM by channel | yes | yes | no |
| L2CAP LE channels | yes | no | yes, Android 10+, listener on psm 0 |
| `GattServer` | yes | yes, primary services | yes |
| `Advertiser` | yes | yes, see below | yes, see below |

## Platform notes

### Linux

- BlueZ over the system D-Bus: adapters, scans, devices, pairing, profiles, the GATT server and the advertiser. RFCOMM by channel and L2CAP go to the kernel sockets directly.
- A pairing handler is an `org.bluez.Agent1` the call registers for its own D-Bus connection: the default agent of the system keeps answering every other pairing.
- `connect_rfcomm` and `listen_rfcomm` register an `org.bluez.Profile1`: BlueZ connects and hands over the socket.
- `ScanResult::connectable` stays false: BlueZ does not report it.
- A lost timeout asks BlueZ for `DuplicateData`, which repeats the manufacturer and service data of every advertisement heard: a device advertising neither stays heard only while BlueZ reports a change.

### Windows

- WinRT for adapters, LE scans, devices, pairing, the GATT client, the GATT server and the advertiser; Win32 `bthprops` for discoverability and classic inquiries; Winsock `AF_BTH` for RFCOMM. Every call runs in a multithreaded apartment of its thread.
- `Device` reaches the devices of the default adapter only: another `adapter` index is `not_supported`. Windows connects on demand: `start` opens a GATT session that keeps the link up and discovers the services, `stop` closes them and Windows drops the link once no program uses it. `info` looks the address up as an LE device first, then as a classic one.
- Classic scans run 2.56 seconds inquiry rounds: a `stop` waits for the round in flight.
- A pairing handler answers the custom pairing of Windows: a PIN to provide is `enter_passkey` (6 digits) for an LE device and `enter_pin` for a classic one. `cancel_pairing` cancels the `pair` in flight of that `Device`, `not_found` without one.
- The advertiser publishes manufacturer data and service data only: a local name, service uuids or a connectable advertisement go through a `GattServer`, which advertises the uuid of each service, connectable. `start` fails otherwise, as it does with `connectable` left to its default `true`.
- `ScanResult::connectable` and the address type need Windows 10 2004.
- `AdapterInfo::alias` mirrors `name`, `pairable` mirrors `powered`, `discovering` stays false.
- No L2CAP channel: Windows has none in user mode.

### Android

- Call `sihd::bt::android_::set_context(JavaVM *, jobject context)` (`<sihd/bt/android/context.hpp>`) once before any other call: the shim loads through the class loader of that context. A call before it is `not_initialized`.
- The application declares the permissions and asks for them at run time: `BLUETOOTH_SCAN` (with `neverForLocation`, which hides beacons from scans), `BLUETOOTH_CONNECT` and `BLUETOOTH_ADVERTISE` from Android 12, `BLUETOOTH`, `BLUETOOTH_ADMIN` and `ACCESS_FINE_LOCATION` before it. A missing one is `permission_denied`. `modules/bt/android/` is a complete example: its manifest and its activity, which asks for them, enables Bluetooth and hands the context over.
- Only the user powers the adapter on, makes it discoverable or answers a pairing: the matching calls are `not_supported`. Android hides the address of the adapter: `list_adapters` reports `02:00:00:00:00:00`. `AdapterInfo::alias` mirrors `name`, `pairable` mirrors `powered`.
- `listen_l2cap` takes psm 0 only: Android picks the psm, another one is `not_supported`.
- `Device` connects as an LE device, asks for a 517 bytes MTU and runs one GATT operation at a time.
- The advertiser refuses a `local_name`: Android only advertises the device name the user set.
- A closing L2CAP channel can hand the reader 5 zero bytes before the end of stream: a protocol that frames its messages ignores them.

## Example

```cpp
#include <thread>

#include <sihd/bt/Device.hpp>
#include <sihd/bt/Scanner.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;
using namespace sihd::bt;

SIHD_NEW_LOGGER("demo");

// prints the readable characteristics of the first device advertising a heart rate service
void explore()
{
    Scanner scanner("scanner");
    scanner.set_filter(ScanFilter {.transport = Transport::le,
                                   .uuids = {"0000180d-0000-1000-8000-00805f9b34fb"},
                                   .min_rssi = std::nullopt});
    if (!scanner.scan_for(Duration(time::sec(5))) || scanner.results().empty())
        return;

    Device device("device");
    device.set_address(scanner.results().begin()->first);
    std::jthread connection([&device] { device.start(); });
    if (!device.wait_ready(Duration(time::sec(30))))
        return;

    auto services = device.services();
    for (const GattService & service : services.value_or(std::vector<GattService> {}))
    {
        for (const GattCharacteristic & characteristic : service.characteristics)
        {
            if ((characteristic.properties & GattCharacteristic::read) == 0)
                continue;
            auto value = device.read_characteristic(characteristic);
            SIHD_LOG(info, "{}: {} bytes", characteristic.uuid, value ? value->size() : 0);
        }
    }
    device.stop();
}
```

`modules/bt/demo/bt_demo.cpp` is an interactive tool built on every platform (`make m=bt demo=1`): scan, connect, explore, read, write, subscribe, pair, chat over RFCOMM or L2CAP, serve GATT and advertise. On Android it is the `bt_demo.apk` terminal.

## Scripting

Python (`sihd.bt`) and Lua (`sihd.bt`) bind the whole API with the C++ names, nested types included (`Device.Event`, `Scanner.Event`, `PairingRequest.Kind`). A failed call raises: `sihd.Error` in Python, the bound error in Lua. Observers and handlers called from the backend threads take the GIL in Python and run through the Lua thread runner in Lua.
