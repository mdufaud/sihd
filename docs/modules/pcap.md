# pcap

Packet capture, packet injection and capture-file I/O on top of libpcap. It offers live capture through a blocking service that delivers packets to observers, raw frame injection on a capture handle, reading and writing of pcap and pcapng files, and enumeration of capture interfaces.

| | |
|---|---|
| Depends on | `net` |
| External libraries | libpcap |
| Platforms | all except web; on Android live capture needs root, see the VPN capture demo |
| Build | `make m=pcap` |
| Header | `#include <sihd/pcap/Sniffer.hpp>` (namespace `sihd::pcap`) |

## Capabilities

### Live capture

- `Sniffer`: `Configurable`, `IReaderTimestamp`, blocking service and `Observable<Sniffer>`. Typical sequence, from the demo: `open(source)`, configure, `activate()`, `set_filter(bpf)`, `sniff()`, `close()`.
  - Lifecycle: `open`, `close`, `is_open`, `activate`, `is_active`, `pollable_fd` (`Result<int>`, `not_found` without a selectable fd, `not_supported` on Windows).
  - Reading: `sniff` (one buffer of packets), `read_next` (one packet), `get_read_data`, `get_read_timestamp`, `packet_len` (length on the wire, larger than the data when the snaplen cut it), `array` / `data`. `interrupt()`, from any thread, makes a blocked `read_next` or `sniff` return without a packet: on Linux, libpcap keeps polling past the `timeout` of a quiet interface.
  - Injection: `inject(frame)` sends a raw link-layer frame (Ethernet header included on `DLT_EN10MB`) and returns the number of bytes sent. Errors: `not_initialized` when the handle is not open or not activated, `io_error` with the libpcap message otherwise. It needs the same privileges as capturing (`CAP_NET_RAW` on Linux). A libpcap handle is not thread-safe: while `start()` captures on another thread, inject from a second `Sniffer` opened on the same device.
  - Capture options: `set_filter` (BPF), `set_promiscuous`, `set_monitor`, `set_immediate`, `set_nonblock`, `set_timeout`, `set_snaplen`, `set_buffer_size`, `set_direction`, `set_datalink`, `set_timestamp_type`, `set_timestamp_nano`, `set_max_sniff`, `set_linux_protocol`.
  - Queries: `datalinks`, `datalink`, `timestamp_types`, `timestamp_type`, `snaplen`, `can_monitor`, `is_nonblock`, `timestamp_nano`, `timestamp_micro`, `stats` (`received`, `dropped`, `if_dropped`), `error`.

### Capture files

- `PcapReader`: reads pcap and pcapng files, by path or `FILE *`, with micro or nano timestamp precision (`open_micro_precision`, `open_nano_precision`). `read_next` returns `false` at end of file, `get_read_data` and `get_read_timestamp` give the current packet. Metadata: `snaplen`, `datalink`, `major_version`, `minor_version`, `is_swapped`, `packet_len`, `packet_caplen`, `file`, `error`.
- `PcapWriter`: writes packets to a file. `open(path[, datalink[, snaplen]])`, `write` (with no timestamp, a `Timestamp`, or seconds and microseconds), `set_snaplen`, `set_datalink`, `flush`, `pos`, `file`, `error`.

### Capture device

- `DevCapture`: a `core` device capturing an interface (`interface` conf) or reading a capture file (`file` conf), one of them. By default it leaves the interface as it is set: `promiscuous` and `monitor` are off unless configured. `immediate` (default true), `timeout` (ms, default 100) and `stats_period` (ms, default 1000, for `kernel_received` and `kernel_dropped`) tune a live capture. `start` opens the source and fails when it cannot; nothing is read until `true` is written to its `activate` input, `false` pauses it.
  - Outputs: `packet` (one `CapturedPacket` per packet, written even when identical to the previous one), `datalink` (written at start), `status` (`DevCapture::Status`: `idle`, `running`, `paused`, `end_of_file`, `failed`), `kernel_received` and `kernel_dropped` (live capture, once a second).
  - A channel timestamp is its write time: the capture time travels in the packet.
  - `realtime` conf: a file is replayed at the pace it was captured, the time between two packets is waited before the second one. Off by default, a file is read as fast as its consumers take it.
  - Live capture waits 100 ms at most per read, in immediate mode. A file ends with `end_of_file`, a read error with `failed`: the capture thread stops in both cases.
- `CapturedPacket`: a packet and its capture metadata in one buffer. `CapturedPacket(timestamp, original_length, datalink, data).encode()` writes the capture time in nanoseconds, the length on the wire and the datalink, in host byte order, then the bytes; `CapturedPacket::decode(buffer)` reads them back (`invalid_argument` when shorter than `header_size`), its `data()` pointing into the buffer.

### Interfaces

- `PcapInterfaces`: enumerates capture devices. `find`, `ifaces`, `names`, `status`, `error`.
- `PcapIFace`: one device with `name`, `description`, `addresses` (`PcapAddress`: `addr`, `netmask`, `broadaddr`, `dstaddr`), status flags (`up`, `running`, `loopback`, `wireless`, `connected`, `disconnected`, `can_be_connected`) and `dump`.

### Utilities

- `utils::init`: libpcap initialization options.
- `utils::lookupnet`: IPv4 network address and mask of a device.
- `utils::version`, `utils::status_str`.
- `utils::datalink`, `utils::datalink_str`, `utils::datalink_desc` and the matching `timestamp_type*` and `is_*` checks: conversions between libpcap link-type and timestamp-type codes and their names.

## Example

Adapted from `modules/pcap/test/TestPcap.cpp`.

```cpp
#include <sihd/pcap/PcapReader.hpp>
#include <sihd/util/Array.hpp>

using namespace sihd::pcap;

size_t count_packets(const std::string & path)
{
    PcapReader reader("pcap-reader");
    size_t count = 0;

    if (!reader.open(path).has_value())
        return count;
    sihd::util::ArrCharView view;
    for (auto next = reader.read_next(); next.has_value() && next.value(); next = reader.read_next())
    {
        reader.get_read_data(view);
        ++count;
    }
    reader.close();
    return count;
}
```

Adapted from `modules/pcap/test/linux/TestSnifferInject.cpp`.

```cpp
#include <sihd/pcap/Sniffer.hpp>
#include <sihd/util/Error.hpp>

using namespace sihd::pcap;

sihd::util::Result<size_t> send_frame(const std::string & device, const std::string & frame)
{
    Sniffer injector("injector");

    auto opened = injector.open(device);
    SIHD_UNEXPECTED_RETURN(opened);
    auto activated = injector.activate();
    SIHD_UNEXPECTED_RETURN(activated);
    return injector.inject({frame.data(), frame.size()});
}
```

## Android capture without root

An app has no `CAP_NET_RAW` and SELinux forbids it from opening packet sockets, so `Sniffer` cannot open a device. The `vpn_capture` demo APK (`make m=net,pcap demo=1 platform=android`) captures through a `VpnService` instead:

- `VpnActivity` asks the VPN consent, `CaptureVpnService` builds the tun (all IPv4 and IPv6 routes, the app itself excluded) and passes its descriptor to the native `main` as `--tun-fd`;
- `net::linux_::TunRelay` relays the traffic from the start, commands typed in the terminal choose what happens to its packets:
  - `start` prints one line per packet: direction (`>` sent, `<` received), protocol, remote address and port, TCP flags, size;
  - `capture` prints them too and `PcapWriter` writes them to `files/capture.pcap` with the `RAW` link type, flushed after every packet; `start` closes the file;
  - `stop` or the VPN revoked ends the relay;
- `adb exec-out run-as sihd.pcap.demo.vpn_capture cat files/capture.pcap > capture.pcap` pulls the file of the debug APK.

Outgoing packets are the apps' own. Incoming TCP headers are written by the relay: the payload is the server's, sequence numbers, window, options and TTL are not.

## Scripting

Not exposed to Lua or Python.
