# wifi

Wifi interfaces: the current association, the access points the radio heard, scans, connections and the networks a backend remembers. Reads go to the kernel: nl80211 on Linux, wlanapi on Windows. Every action goes through the one daemon that manages the interface: NetworkManager, iwd or wpa_supplicant on Linux, the WLAN AutoConfig service on Windows. The library never talks around that daemon.

| | |
|---|---|
| Depends on | `util`, `sys`, `net` |
| External libraries | none (Linux: D-Bus through `sys`, nl80211 through `net`; Windows: `wlanapi.dll` loaded at runtime) |
| Platforms | Linux, Windows |
| Build | `make m=wifi` |
| Header | `#include <sihd/wifi.hpp>` |

## Capabilities

### Link

- `link(interface_name)`: the association of a wifi station as `Link`: `ssid`, `bssid`, `signal_dbm`, `frequency_mhz`, `tx_bitrate_bps` (0 when unknown). `not_found` when the interface is not a wifi interface or not an associated station.
  - Linux: nl80211, `not_supported` without a cfg80211 wifi stack.
  - Windows: the signal falls back to the link quality (0..100 mapped onto -100..-50 dBm) and the frequency stays 0 when the BSS list lacks the access point.

### Access points

- `access_points(interface_name)`: what the radio heard lately, as `AccessPoint`: `ssid`, `bssid`, `signal_dbm`, `frequency_mhz`, `security`, `seen_ago`, `associated`. It never triggers a scan and needs no privilege.
  - Linux: the nl80211 scan dump. The kernel drops the entries heard more than 30 seconds ago, the associated one excepted.
  - Windows: `WlanGetNetworkBssList`. `seen_ago` stays 0.
- `scan(interface_name, timeout)`: asks the backend for a scan and returns once `access_points` holds its results. `timeout` when it does not finish in time.
- `security_of(information_elements, privacy)`: classifies the information elements of a beacon or probe response, `privacy` being the capability bit. `Security` is `open`, `owe`, `wep`, `personal` (PSK or SAE: a passphrase is needed), `enterprise` (802.1X) or `unknown` (an element runs past the buffer). RSN wins over the WPA vendor element.

### Backends

- `backend(interface_name)`: the daemon managing the interface, as `Backend::network_manager`, `iwd`, `wpa_supplicant` or `wlan_service` (Windows). `not_found` when none does.
- Linux selection order, per interface:
  1. NetworkManager, when its device for the interface is managed and of the wifi type. NetworkManager runs over iwd or wpa_supplicant, which it drives: they are never called behind its back.
  2. iwd, when one of its stations carries the interface name.
  3. wpa_supplicant, when it controls the interface.
- A daemon is probed with `NameHasOwner` and the D-Bus connection disables auto-start: probing never starts an activatable daemon. A started iwd would grab the radio from the running supplicant.
- Every call opens a D-Bus connection and detects the backend again, a few hundred microseconds on a local bus. The calls per object (access points, profiles, BSSs) leave together through `call_all`, so a list costs one round trip.

| | NetworkManager | iwd | wpa_supplicant | Windows |
|---|---|---|---|---|
| scan | `RequestScan`, waits for `LastScan` | `Station.Scan`, waits for `Scanning` false | `Interface.Scan`, waits for `ScanDone` | `WlanScan`, waits for the scan notification |
| connect | `ActivateConnection` or `AddAndActivateConnection`, waits for the device state | `Network.Connect`, the passphrase served by an agent | `AddNetwork` + `SelectNetwork`, waits for `completed`, fails on a disconnect once the association started | `WlanSetProfile` (never overwriting a profile) + `WlanConnect`, waits for the connection notification |
| known networks | wifi connection profiles | known networks | configured networks | profiles |
| unprivileged access | polkit, active session | `root`, `wheel` or `network` group (Arch policy) | `root` only (Arch policy) | location consent since Windows 11 24H2 |

- `security_str`, `backend_str`, `state_str`: the names of the enums.

### Connections

- `state(interface_name)`: `State::disconnected`, `connecting`, `connected` or `disconnecting`.
- `connect(interface_name, ssid, timeout)`: a known network, or an open (or OWE) one. `connect(interface_name, ssid, passphrase, timeout)`: a network the backend does not know yet. Both block until connected. A failure leaves the known networks as they were: the profile created for the attempt is removed. Every backend fails the same way:
  - `not_found` when the backend did not see the SSID: scan first;
  - `invalid_argument` for an SSID outside 1 to 32 bytes, a passphrase outside 8 to 63 characters, an unknown secured network without passphrase, or an open network with one;
  - `already_exists` for a passphrase given to a known SSID: `forget` it first. A passphrase is never replaced in place, iwd has no way to do it without forgetting the network;
  - `not_supported` for an 802.1X or WEP network the backend does not know;
  - `timeout`, or an `io_error` carrying the backend reason, a refused passphrase included.
- `disconnect(interface_name)`: a disconnected interface is no failure.
- `known_networks(interface_name)`: the networks the backend remembers, as `KnownNetwork` (`ssid`, `security`). NetworkManager and iwd keep them for every interface. `forget(interface_name, ssid)` removes them, `not_found` when none matches.
- Persistence follows the backend: NetworkManager, iwd and Windows save a profile; a network added to wpa_supplicant lives until it restarts, unless its own configuration saves it.
- An `AccessDenied` from the D-Bus policy, or a missing location consent on Windows, gives `permission_denied`.

### Monitoring

- `Monitor([run_timeout])` then `open(interface_name)`: subscribes to the backend of the interface, failing like `backend()` for the same interface, `already_exists` once open. `run()` waits up to the run timeout (a negative one waits for an event), drains what arrived, notifies its observers and returns false when reading failed or nothing is open. `events()` holds `Monitor::Event` items: `Type::scan_done`, or `Type::state_changed` with the new `state`. A state equal to the last one reported is dropped.
- The backend is chosen once: a daemon restarted later is not followed.

## Example

```cpp
#include <fmt/format.h>

#include <sihd/util/time.hpp>
#include <sihd/wifi.hpp>

using namespace sihd::util;
using namespace sihd::wifi;

void list_networks(const std::string & interface_name)
{
    auto scanned = scan(interface_name, Duration(time::sec(15)));
    if (!scanned)
        fmt::print("scan: {}\n", scanned.error().message);

    auto heard = access_points(interface_name);
    if (!heard)
        return;
    for (const AccessPoint & access_point : *heard)
        fmt::print("{} {} dBm '{}'\n", access_point.bssid, access_point.signal_dbm, access_point.ssid);
}
```

Adapted from `modules/wifi/demo/wifi_demo.cpp`.

## Scripting

Exposed to Lua (`LuaWifiApi`) and Python (`PyWifiApi`) under `sihd.wifi`: `Security`, `Backend` and `State`, `Link`, `AccessPoint`, `KnownNetwork`, `link`, `access_points`, `security_of` (a string in Lua, `bytes` in Python), `scan`, `backend`, `state`, `connect` with or without a passphrase, `disconnect`, `known_networks`, `forget` and `Monitor` (`run` then `events` of `Monitor.Event`, `Monitor.Event.Type`). Timeouts take a `Duration`, a plain number being nanoseconds: `sihd.util.time.ms(500)`. Failures raise: a `sihd.Error` in Python, a bound `Error` in Lua. In Python the monitor is `Monitor([run_timeout])` then `open(interface_name)`; in Lua it is built with `Monitor.open(interface_name[, timeout])`.
