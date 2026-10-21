# pcap

Packet capture and capture-file I/O on top of libpcap. It offers live capture through a blocking service that delivers packets to observers, reading and writing of pcap and pcapng files, and enumeration of capture interfaces.

| | |
|---|---|
| Depends on | `net` |
| External libraries | libpcap |
| Platforms | all except Android |
| Build | `make m=pcap` |
| Header | `#include <sihd/pcap/Sniffer.hpp>` (namespace `sihd::pcap`) |

## Capabilities

### Live capture

- `Sniffer`: `Configurable`, `IReaderTimestamp`, blocking service and `Observable<Sniffer>`. Typical sequence, from the demo: `open(source)`, configure, `activate()`, `set_filter(bpf)`, `sniff()`, `close()`.
  - Lifecycle: `open`, `close`, `is_open`, `activate`, `is_active`, `pollable_fd`.
  - Reading: `sniff` (one buffer of packets), `read_next` (one packet), `get_read_data`, `get_read_timestamp`, `array` / `data`.
  - Capture options: `set_filter` (BPF), `set_promiscuous`, `set_monitor`, `set_immediate`, `set_nonblock`, `set_timeout`, `set_snaplen`, `set_buffer_size`, `set_direction`, `set_datalink`, `set_timestamp_type`, `set_timestamp_nano`, `set_max_sniff`, `set_linux_protocol`.
  - Queries: `datalinks`, `datalink`, `timestamp_types`, `timestamp_type`, `snaplen`, `can_monitor`, `is_nonblock`, `timestamp_nano`, `timestamp_micro`, `stats` (`received`, `dropped`, `if_dropped`), `error`.

### Capture files

- `PcapReader`: reads pcap and pcapng files, by path or `FILE *`, with micro or nano timestamp precision (`open_micro_precision`, `open_nano_precision`). `read_next` returns `false` at end of file, `get_read_data` and `get_read_timestamp` give the current packet. Metadata: `snaplen`, `datalink`, `major_version`, `minor_version`, `is_swapped`, `packet_len`, `packet_caplen`, `file`, `error`.
- `PcapWriter`: writes packets to a file. `open(path[, datalink[, snaplen]])`, `write` (with no timestamp, a `Timestamp`, or seconds and microseconds), `set_snaplen`, `set_datalink`, `flush`, `pos`, `file`, `error`.

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

Adapted from `modules/pcap/test/TestPcap.cpp`.

## Scripting

Not exposed to Lua or Python.
