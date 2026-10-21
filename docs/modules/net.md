# net

Socket networking over IP and Unix domain sockets. It provides address handling, raw sockets, TCP, UDP, TLS over TCP, ICMP ping, DNS resolution and network interface enumeration. Most network classes are `Named` services that can be run through the service lifecycle and observed through `Handler` callbacks, and the `Device*` wrappers expose them as `core` devices.

| | |
|---|---|
| Depends on | `sys`, `core`, `crypto` |
| External libraries | openssl (through `crypto`, for TLS); iphlpapi on Windows |
| Platforms | all except web (Emscripten) |
| Build | `make m=net` |
| Header | `#include <sihd/net/TcpClient.hpp>` (one header per class, namespace `sihd::net`) |

## Capabilities

### Addresses

- `IpAddr`: IPv4/IPv6 endpoint (host, port, `sockaddr`). Built from `(host, port)`, from a `sockaddr*`, or with `IpAddr::localhost(port, ipv6)`. Key methods: `str`, `hostname`, `fetch_hostname`, `has_ip`, `empty`, `addr`, `addr_len`, `subnet`, `set_subnet_mask`, `is_same_subnet`.
- `Subnet`: netmask, network, host range and broadcast of an `IpAddr`, as returned by `IpAddr::subnet()`.
- `ip::` helpers: `is_valid_ip`, `is_private_host`, `public_addresses`, `to_str` (for `sockaddr` variants), `domain` / `socktype` / `protocol` name conversions and their `*_str` counterparts, `is_valid_netmask`.

### Sockets

- `Socket`: wrapper over a file descriptor (IPv4, IPv6, Unix). Opens with `open`, then `bind`, `connect`, `listen`, `accept`, `send`/`send_all`, `receive`, `send_to`/`receive_from`, and `*_unix` variants. Static and member option setters/getters: TCP nodelay, blocking, reuse address/port, keepalive, broadcast, TTL, buffer sizes, recv timeout, bind to device. Multicast: `join_multicast`, `leave_multicast`, `set_multicast_ttl`, `set_multicast_loop`.
- `TlsSocket`: `Socket` with TLS. `set_tls_context`, `set_tls_server_name`, `tls_accept`, `tls_accept_step`, `tls_active`.
- `TlsConnection`: low-level TLS session on a `Socket`. `init`, `connect`, `accept`, `accept_step`, `read`, `write`, `session`, `set_session` (resumption), `shutdown`.

### TCP

- `TcpClient`: `INetReceiver` and `INetSender`, `Observable<INetReceiver>`, runnable as a blocking service. `open_socket`, `connect`, `reconnect`, `open_and_connect` (IP or Unix path), `send`, `send_all`, `receive`, `poll`, `set_poll_timeout`, `set_tls_context`.
- `TcpServer`: `INetServer` and blocking service. `open_and_bind` (IP or Unix path), `accept_client`, `set_server_handler`, `set_queue_size`, `set_poll_limit`, `add_client_read` / `add_client_write` and their `remove_*` counterparts.
- `BasicServerHandler`: `INetServerHandler` that tracks clients (`Client`) and their read, write and new-connection activity. Configuration: `set_max_clients`, `set_idle_timeout`, `set_tls_accept_timeout`, `set_tls_context`. Operations: `send_to_client`, `remove_client`, `clients`, `client`, `client_count`.

### UDP

- `UdpSender`: `INetSender`. `open_and_connect` then `send` / `send_all`, or `send_to` / `send_to_all` for an address per call.
- `UdpReceiver`: `INetReceiver`, observable, runnable as a blocking service. `open_and_bind`, `set_reuseaddr`, `receive` (with or without sender address), `poll`, `set_poll_timeout`.

### ICMP

- `IcmpSender`: raw ICMP echo sender. `open_socket` (IPv4/IPv6) or `open_socket_unix`, then `set_type`, `set_code`, `set_ttl`, `set_id`, `set_seq`, `set_data_size`, `send_to`, `poll`. `response()` returns the last `IcmpResponse`.
- `Pinger`: observable ping driver on top of `IcmpSender`. Configure with `set_timeout`, `set_interval`, `set_ping_count`, `set_ttl`, `set_client`. Results in `event()` (`PingEvent`) and `result()` (`PingResult`, with `packet_loss` and `str`).

### DNS

- `dns::find`: resolve a host to an `IpAddr` (IPv4 by default, IPv6 with the flag).
- `dns::lookup`: all `DnsEntry` results (address, socket type, protocol) and canonical hostname, as `DnsLookup`.
- `dns::lookup_async`: same as `lookup`, returning a `std::future`.

### Interfaces

- `NetInterface`: one network interface. `NetInterface::get_all_interfaces()` returns them keyed by name. Accessors: `name`, `mac_addr`, `addr4`/`netmask4`/`addr6`/`netmask6`, `extra_addr4`/`extra_addr6` (point-to-point or broadcast), and flag queries such as `up`, `running`, `loopback`, `broadcast`, `point2point`, `promisc`, `supports_multicast`.

### Abstractions

- `INetSender`: `send`, `send_all`, `close`.
- `INetReceiver`: `receive` (with or without `IpAddr`), `close`.
- `INetServer`: `accept_client` and client read/write registration, implemented by `TcpServer`.
- `INetServerHandler`: callbacks for the server loop: no activity, activity, new client, client read, client write, after activity.

### Devices

- `DeviceTcpClient`, `DeviceTcpServer`, `DeviceUdpSender`, `DeviceUdpReceiver`: `core::Device` wrappers. They are configured with `set_host`, `set_port`, `set_unix_path`, `set_buffer_capacity`, `set_poll_timeout`, and connect to channels (`handle(Channel *)`) or to a `INetReceiver`/`BasicServerHandler`.

## Example

```cpp
#include <sihd/net/UdpReceiver.hpp>
#include <sihd/net/UdpSender.hpp>
#include <sihd/util/Array.hpp>

using namespace sihd::net;

void udp_send_and_receive()
{
    UdpReceiver receiver("udp-receiver");
    UdpSender sender("udp-sender");
    sihd::util::ArrInt message = {1, 2, 3, 4, 5, 6};
    sihd::util::ArrInt received(message.size());

    if (!receiver.open_and_bind(IpAddr::localhost(4242)).has_value())
        return;
    if (!sender.open_and_connect(IpAddr::localhost(4242)).has_value())
        return;
    if (sender.send_all(message).has_value() && receiver.poll(1000))
        receiver.receive(received);
    receiver.close();
    sender.close();
}
```

Adapted from `modules/net/test/TestUdp.cpp`.

## Scripting

Exposed to Lua (`LuaNetApi`) and Python (`PyNetApi`).
