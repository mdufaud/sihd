# net

Socket networking over IP and Unix domain sockets. It provides address handling, raw sockets, TCP, UDP, TLS over TCP, ICMP ping, DNS resolution, network interface enumeration and connection detection: link kinds, routes, VPN tunnels, DNS servers and change monitoring. Most network classes are `Named` services that can be run through the service lifecycle and observed through `Handler` callbacks, and the `Device*` wrappers expose them as `core` devices.

| | |
|---|---|
| Depends on | `sys`, `core`, `crypto` |
| External libraries | openssl (through `crypto`, for TLS); iphlpapi on Windows |
| Platforms | all except web (Emscripten) |
| Build | `make m=net` |
| Header | `#include <sihd/net/TcpClient.hpp>` (one header per class, namespace `sihd::net`) |

## Capabilities

### Addresses

- `IpAddr`: IPv4/IPv6 endpoint (host, port, `sockaddr`). Built from `(host, port)`, from a `sockaddr*`, with `IpAddr::localhost(port, ipv6)`, or from raw `in_addr`/`in6_addr` bytes with `IpAddr::from_bytes(family, data, size)`. Key methods: `str`, `hostname`, `fetch_hostname`, `has_ip`, `empty`, `addr`, `addr_len`, `subnet`, `set_subnet_mask`, `is_same_subnet`.
- `Subnet`: netmask, network, host range and broadcast of an `IpAddr`, as returned by `IpAddr::subnet()`.
- `ip::` helpers: `is_valid_ip`, `is_private_host`, `public_addresses`, `to_str` (for `sockaddr` variants), `mac_str` (`AA:BB:CC:DD:EE:FF`), `domain` / `socktype` / `protocol` name conversions and their `*_str` counterparts, `is_valid_netmask`.

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
- `dns::servers(resolver_timeout)`: the name servers the system resolver queries, as `DnsServer` (`addr`, `interface_name`, empty for a server of every interface).
  - Linux: `/etc/resolv.conf`. A `fe80::1%wlan0` entry carries its interface. When resolv.conf only lists the systemd-resolved stub (127.0.0.53/54), the real servers come from `org.freedesktop.resolve1` over dbus, per interface, within `resolver_timeout` (`dns::default_resolver_timeout`, 2 s).
  - Windows: the DNS servers of every adapter that is up.
  - Android: `not_supported` (netd resolves, there is no resolv.conf).

### Interfaces

- `NetInterface`: one network interface. `NetInterface::get_all_interfaces()` returns them keyed by name (the friendly name on Windows). Accessors:
  - `name`, `mac_addr`, `index`, `mtu`, `link_speed_bps` (0 when unknown);
  - `addresses()`: every address of the interface as `Address` (`addr`, `netmask`, `extra_addr`), secondary IPv4 addresses included: Linux reports those under an `eth0:1` label, which joins `eth0` instead of becoming an interface. `addr4`/`netmask4`/`addr6`/`netmask6` and `extra_addr4`/`extra_addr6` (point-to-point or broadcast) keep only the last primary address of each family;
  - `kind()`: `NetInterface::Kind` among `loopback`, `ethernet`, `wifi`, `cellular`, `tunnel`, `ppp`, `bridge`, `unknown`, with `kind_str`;
  - `hardware()`: true when a physical device backs the link, false for loopback, tunnels, bridges, veth, TAP;
  - flag queries such as `up`, `running`, `loopback`, `broadcast`, `point2point`, `promisc`, `supports_multicast`.
- How `kind` and `hardware` are detected:
  - Linux: `/sys/class/net/<name>`. `tun_flags` means tunnel (tun and tap), then `DEVTYPE` in `uevent` (`wlan`, `wwan`, `bridge`, `wireguard`, `ovpn`, `ppp`), then the ARPHRD `type`. `hardware` is the `device` link. sysfs shows the network namespace it was mounted from: when its `ifindex` does not match the link, `kind` stays `unknown` (a loopback keeps `loopback`, from its flag), `hardware()` is false and `mtu`/`link_speed_bps` are 0.
  - Windows: `IfType` (71 wifi, 6 ethernet, 243/244 cellular, 23 ppp, which also covers the built-in VPN, 131 and 53 tunnel, the latter declared by wintun, WireGuard-NT, TAP-Windows6 and ovpn-dco). `hardware` is `HardwareInterface` from `GetIfEntry2`. Not verified on a real Windows: Wine reports `HardwareInterface` for everything but loopback, and the IfType of Hyper-V vEthernet or AnyConnect adapters is unknown.
  - Android apps cannot read sysfs (AOSP sepolicy, not verified on a device): every kind but loopback is `unknown`, `hardware()` is false, mtu and speed are 0.

### Routes

- `route::find(destination)`: the `Route` the system picks for a destination: `interface_name`, `gateway` (empty for an on-link route), `source` (the local address used) and `metric`. Policy routing applies, so a wg-quick tunnel (fwmark rules) or an OpenVPN `def1` tunnel (0/1 + 128/1 routes) shows up here. A link-local IPv6 destination with a scope id is looked up on that link. `not_found` when the destination is unreachable.
- `route::defaults_ipv4()` / `route::defaults_ipv6()`: every default route of the family, every routing table included, lowest metric first.
- `metric` is the effective one: the route metric plus the interface metric on Windows. Linux IPv4 lookups do not report it: `find` leaves it at 0 there.
- Linux: rtnetlink, no bind, so an Android app can use it. Windows: `GetBestInterfaceEx` + `GetBestRoute2` and `GetIpForwardTable2`.

### Monitoring

- `NetMonitor`: `Observable<NetMonitor>` and `IRunnable`, shaped like `sys::FileWatcher`. The constructor subscribes to the system notifications and throws when refused.
- `run()` waits up to the run timeout (`set_run_timeout`, 100 ms by default), drains every pending change into `events()` and, only when something changed, notifies the observers once, on the calling thread. `events()` holds the changes of the last run only. It returns false only when reading the notifications failed. Run it in a loop or a `StepWorker`.
- `NetMonitorEvent`: `type` (`link`, `address`, `route`, `overflow`), `removed`, `interface_name` and the `address` of an address event. An `overflow` event means notifications were dropped: re-read everything. A connection change sends a burst of events, so re-query `route::find` on them instead of tracking each one. `interface_name` is the name the system still knows (a removed link keeps it on Linux), empty when it can no longer be resolved, as for the routes and addresses of a link already gone.
  - Linux: rtnetlink multicast groups (an Android app may not bind them: the constructor throws).
  - Windows: `NotifyIpInterfaceChange`, `NotifyUnicastIpAddressChange` and `NotifyRouteChange2`. Wine accepts them but never notifies.

### Netlink (Linux)

- `sihd/net/linux/netlink.hpp`: the netlink socket behind routes and monitoring, public for other families (`wifi` drives nl80211 with it). `linux_::netlink::Netlink` (`open`, `subscribe`, `set_nonblocking`, `request`, `receive`), `Request` (`set_header`, `add_attribute`, `add_u32`, `add_string`), and `Message` / `Attribute` read through free decoders (`header`, `attributes`, `nested`, `u8`, `u16`, `u32`, `str`, `ip`).

### Recipes

- Main connection: `route::find` towards any public address, then look its `interface_name` up in `get_all_interfaces()`. The library picks no address: the caller chooses one.
- VPN:
  - full tunnel: the main connection is a `tunnel` or `ppp` link;
  - split tunnel: a running `tunnel` or `ppp` link exists while the main connection is another link;
  - the uplink under a full tunnel is the first default route on a link that is neither;
  - `hardware()` alone is no VPN sign: bridges, veth and container links have no hardware either.
- DNS leak: `route::find(server.addr)` for each `dns::servers()` entry gives the link its queries take.
- Limits:
  - policy-based IPsec (strongSwan without an xfrm interface) creates no link and no route: it is invisible here;
  - a VPN adapter reported with another IfType on Windows (Hyper-V-like virtual ethernet, AnyConnect) is not recognized;
  - a captive portal or a dead internet access needs a real probe (an HTTP request, `Pinger`).
- `net_interfaces_demo` prints all of it, and `net_interfaces_demo watch` prints the changes.

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

```cpp
#include <sihd/net/IpAddr.hpp>
#include <sihd/net/NetInterface.hpp>
#include <sihd/net/route.hpp>

using namespace sihd::net;

bool goes_through_vpn(const IpAddr & destination)
{
    auto interfaces = NetInterface::get_all_interfaces();
    auto found = route::find(destination);
    if (!interfaces || !found || !interfaces->contains(found->interface_name))
        return false;
    const NetInterface & link = interfaces->at(found->interface_name);
    return link.kind() == NetInterface::Kind::tunnel || link.kind() == NetInterface::Kind::ppp;
}
```

Adapted from `modules/net/test/linux/TestNetNamespace.cpp` and `modules/net/demo/net_interfaces_demo.cpp`.

## Scripting

Exposed to Lua (`LuaNetApi`) and Python (`PyNetApi`). Both bind `NetInterface`, `route`, `dns.servers` and `NetMonitor`, whose `run()` is read through `events()`: no observer crosses into the scripts. Both expose the same `NetInterface` accessors. Python raises `sihd.Error` (a `RuntimeError`) on failures, the refused `NetMonitor` constructor included. In Lua, failures raise a bound `Error` (`message`, `code`) that `pcall` catches, the kinds are `NetInterface.Kind`, the event types `NetMonitorEventType`, and the monitor is built with `NetMonitor.open([timeout])`, which raises when refused.
