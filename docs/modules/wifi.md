# wifi

Wifi interfaces: the current association, the access points the radio heard, scans, connections and the networks a backend remembers. Reads go to the kernel: nl80211 on Linux, wlanapi on Windows. Every action goes through the one daemon that manages the interface: NetworkManager, iwd or wpa_supplicant on Linux, the WLAN AutoConfig service on Windows. The library never talks around that daemon. It also decodes captured 802.11 frames (radiotap, headers, management bodies, information elements), a purely passive reading that needs neither a radio nor a daemon.

| | |
|---|---|
| Depends on | `net` |
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
- `security_of(information_elements, privacy)`: classifies the information elements of a beacon or probe response, `privacy` being the capability bit. `Security` is `open`, `owe`, `wep`, `personal` (PSK or SAE: a passphrase is needed), `enterprise` (802.1X) or `unknown` (an element runs past the buffer, or an RSN or WPA body is cut short). RSN wins over the WPA vendor element.

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

### Frame decoding

A byte-by-byte primer and the vocabulary are in [wifi-frames.md](../protocols/wifi-frames.md).

Passive reading of captured frames: nothing is sent and the radio is not configured. The bytes come from `pcap::Sniffer` or `pcap::PcapReader` (`wifi` does not depend on `pcap`), on an interface already put in monitor mode. Decoders take an `ArrUByteView`, hold no platform-specific code (same on Linux and Windows) and do not copy the bytes they read: the byte fields of a result (`Frame::body`, `Radiotap::payload`, `information_elements`, the data of an `Element`) are views into the caller's buffer, which must outlive the result, while numbers, addresses, suites and strings are copies. A truncated or malformed input gives `invalid_argument`.

- `dissect_radiotap(bytes)` for datalink 127 (`DLT_IEEE802_11_RADIO`) and `dissect_80211(bytes)` for datalink 105 (`DLT_IEEE802_11`) give a `Packet`: an optional `Radiotap` and a `Frame`. The caller picks one from `Sniffer::datalink()`. With the radiotap padding flag set, `Frame::body` skips the padding. A datalink 1 (ethernet) capture holds no 802.11 frame.
- `parse_radiotap(bytes)`: `Radiotap` with `tsft_us`, `rate_kbps`, `mcs` (index, bandwidth, guard interval, coding, and the rate they give), `frequency_mhz`, `channel_flags`, `signal_dbm`, `noise_dbm`, `antenna`, one `AntennaSignal` per extra antenna, the flags, `header_size` and the `payload`. Fields are read with their natural alignment from the start of the header, across chained presence words and radiotap namespaces; a vendor namespace is skipped by its length. A field the parser cannot size stops the walk and keeps what was read. A frame flagged with a trailing FCS loses it from `payload`, which assumes the capture was not cut by its snaplen; `bad_fcs` is reported, not acted on.
- `parse_frame(bytes)`: `Frame` with the type, subtype, frame control flags, `duration`, `addr1` to `addr4`, `sequence_number`, `fragment_number`, `qos_tid`, `amsdu`, `ht_control`, `header_size` and `body`. Header sizes follow the type: control frames 10 or 16 bytes, management and data 24, plus 6 for the fourth address, 2 for QoS and 4 for HT control. A protocol version other than 0 is `invalid_argument`, an extension frame `not_supported`. `management_subtype(frame)` and `control_subtype(frame)` give the subtype as its enum, empty for another type or a reserved value.
- `addresses(frame)`: `Addresses` with `ra`, `ta`, `bssid`, `sa` and `da` as the type, ToDS and FromDS define them. A role the frame has none for is empty (no BSSID in a WDS frame, no transmitter in an ACK or a control wrapper). `is_group` and `is_locally_administered` test the first octet of a `MacAddress`: multicast or broadcast, and the randomized client addresses. `parse_mac` reads the `AA:BB:CC:DD:EE:FF` text back into a `MacAddress`. `management_subtype(frame)` and `control_subtype(frame)` give the subtype as its enum, empty for another type or a reserved value.
- Management bodies (`Frame::body` of a management frame): `parse_beacon` and `parse_probe_response` (`Beacon`: `timestamp_us`, `interval_tu`, `capability`, `ess`, `ibss`, `privacy`), `parse_assoc_request`, `parse_reassoc_request`, `parse_assoc_response`, `parse_authentication`, `parse_reason` (deauthentication and disassociation: the reason code) and `parse_action` (`Action`: `category` and the raw `body`). The others leave the information elements in `information_elements`; a probe request body is only elements.
- `parse_llc_snap(body)`: `Llc` with the `ethertype` (`0x888e` is EAPOL) and the `payload` of a data frame that is neither protected nor an A-MSDU. A protected frame is encrypted and nothing here decrypts it.
- Information elements, in `sihd::wifi::ies`: `elements` walks the id, length, data triplets (`readable_elements` keeps the complete ones of a cut frame) and `find` and `find_vendor` pick one. `ssid`, `channel` (the DS parameter set, else the HT primary channel), `rates` (basic flag included, membership selectors left out), `parse_rsn` (an RSN body), `rsn` and `wpa` (`Rsn`: group, pairwise and AKM `Suite`s, capabilities; `cipher_str` and `akm_str` name a suite) and `parse_wps` (the attributes of the WPS element), `wps_attributes` (the same attributes, uninterpreted), `wps` (`Wps`: version, setup state, lock, manufacturer, model, device name) read the elements. `vendor_of` splits a vendor specific element into OUI, type and data. `security_of` is built on them: when an RSN or a WPA element is repeated, the last one decides. A hidden network may send as many zero bytes as the real name has characters; `ssid` returns them as they are.
- What a device announces, in `sihd::wifi::ies` (`capabilities.hpp`): `ht_capabilities` (802.11n: channel width, guard intervals, STBC, A-MPDU limits, spatial streams), `vht_capabilities` (802.11ac: MPDU length, 160 MHz, beamforming, spatial streams), `country` (code, environment, channel ranges and power), `tim`, `standards` (which of HT, VHT, HE and EHT the elements announce) and `vendors` (every vendor specific element).
- Device signatures, in `sihd::wifi::taxonomy` (`taxonomy.hpp`): `element_signature(information_elements)` and `signature(probe_request_elements, assoc_request_elements)` write the line hostapd's taxonomy and Google's wifi device taxonomy use to name a device model (`wifi4|probe:0,1,50,...|assoc:0,1,...`). The list of known models is not embedded: the application supplies it and compares the lines.

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

Decoding one packet of a datalink 127 capture (the bytes come from `PcapReader::read_next()` or `Sniffer`):

```cpp
#include <fmt/format.h>

#include <sihd/wifi.hpp>

using namespace sihd::util;
using namespace sihd::wifi;

void print_beacon(ArrUByteView bytes)
{
    const Result<Packet> packet = dissect_radiotap(bytes);
    if (!packet || management_subtype(packet->frame) != Frame::ManagementSubtype::beacon)
        return;

    const Result<Beacon> beacon = parse_beacon(packet->frame.body);
    if (!beacon)
        return;
    const std::vector<ies::Element> elements = ies::readable_elements(beacon->information_elements);
    fmt::print("'{}' channel {} {} dBm\n",
               ies::ssid(elements),
               ies::channel(elements).value_or(0),
               packet->radiotap->signal_dbm.value_or(0));
}
```

The same calls run on real captured frames in `modules/wifi/test/TestDissect.cpp`.

## Scripting

Exposed to Lua (`LuaWifiApi`) and Python (`PyWifiApi`) under `sihd.wifi`: `Security`, `Backend` and `State`, `Link`, `AccessPoint`, `KnownNetwork`, `link`, `access_points`, `security_of` (a string in Lua, `bytes` in Python), `scan`, `backend`, `state`, `connect` with or without a passphrase, `disconnect`, `known_networks`, `forget` and `Monitor` (`run` then `events` of `Monitor.Event`, `Monitor.Event.Type`). Timeouts take a `Duration`, a plain number being nanoseconds: `sihd.util.time.ms(500)`. Failures raise: a `sihd.Error` in Python, a bound `Error` in Lua. In Python the monitor is `Monitor([run_timeout])` then `open(interface_name)`; in Lua it is built with `Monitor.open(interface_name[, timeout])`.

The decoders take `bytes` (a string in Lua) and return objects that keep their own copy of them: `Radiotap` (with `Radiotap.AntennaSignal` and `Radiotap.Mcs`), `Frame` (with `Frame.Type`, `Frame.ManagementSubtype`, `Frame.ControlSubtype`), `Addresses`, `Packet`, `Beacon`, `AssocRequest`, `ReassocRequest`, `AssocResponse`, `Authentication`, `Action`, `Llc`, and the functions `dissect_radiotap`, `dissect_80211`, `parse_radiotap`, `parse_frame`, `addresses`, `management_subtype`, `control_subtype`, `parse_mac`, `is_group`, `is_locally_administered` (the last three take the `"AA:BB:CC:DD:EE:FF"` text), `parse_beacon`, `parse_probe_response`, `parse_assoc_request`, `parse_reassoc_request`, `parse_assoc_response`, `parse_authentication`, `parse_reason`, `parse_action`, `parse_llc_snap`. Addresses are `"AA:BB:CC:DD:EE:FF"` strings, an absent one `None` or `nil`, and the byte views (`body`, `payload`, `information_elements`) are copied out as `bytes` or strings. A Python `str` (an ssid, a WPS name) decodes the bytes as UTF-8 and replaces what is not by the replacement character; Lua strings keep the bytes. Python exposes `Frame.Type` and the subtypes as enums (`frame.subtype == int(Frame.ManagementSubtype.beacon)`), Lua as plain tables. The `ies` submodule holds `Id` (the element ids), `Element` (`id` as an integer), `WpsAttribute`, `Rate`, `Suite`, `Rsn`, `Wps`, `HtCapabilities`, `VhtCapabilities`, `Country` (with `Country.Channels`), `Tim`, `Standards`, `Vendor` and the functions `elements`, `readable_elements`, `find` (a raw `Id` in Python, an integer in Lua), `find_vendor`, `vendor_of`, `ssid`, `channel`, `rates`, `parse_rsn`, `rsn`, `wpa`, `parse_wps`, `wps_attributes`, `wps`, `ht_capabilities`, `vht_capabilities`, `country`, `tim`, `standards`, `vendors`, `cipher_str`, `akm_str`, which take the raw elements of a beacon (`Beacon.information_elements`). The `taxonomy` submodule holds `element_signature` and `signature`. In Python, the `ssid` of `Link`, `AccessPoint` and `KnownNetwork` can be set from `bytes`, to build a result by hand.
