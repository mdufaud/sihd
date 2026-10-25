import sihd

# IpAddr value type
addr = sihd.net.IpAddr("127.0.0.1", 4300)
assert(addr.is_ipv4())
assert(not addr.is_ipv6())
assert(addr.has_ip())
assert(addr.port() == 4300)
assert("127.0.0.1" in addr.str())

addr_noport = sihd.net.IpAddr("127.0.0.1")
assert(addr_noport.port() == 0)

# UDP device loopback through a Core
core = sihd.core.Core("core")
sender = sihd.net.DeviceUdpSender("sender", core)
receiver = sihd.net.DeviceUdpReceiver("receiver", core)

assert(sender.set_conf(host="127.0.0.1", port=4300))
assert(receiver.set_conf(host="127.0.0.1", port=4300))
assert(receiver.set_poll_timeout(1))
assert(receiver.set_buffer_capacity(1024))

assert(core.init())

rx = receiver.find_channel("rx")
tx = sender.find_channel("tx")
assert(rx is not None)
assert(tx is not None)

assert(core.start())
assert(receiver.is_running())

waiter = sihd.core.ChannelWaiter(rx)

payload = "hello udp device"
assert(tx.write_array(sihd.util.ArrChar(payload)))

# prev_wait_for: same thread writes then waits, so a packet that lands before this
# call must still count (wait_for would ask for a second one and time out)
assert(waiter.prev_wait_for(sihd.util.time.ms(2000), 1))

out = sihd.util.ArrChar()
out.resize(rx.array().size())
assert(rx.copy_to(out))
assert(out.str() == payload)

assert(core.stop())
assert(not receiver.is_running())

# connection detection
NetInterface = sihd.net.NetInterface
interfaces = NetInterface.get_all_interfaces()
loopbacks = [netif for netif in interfaces.values() if netif.kind() == NetInterface.Kind.loopback]
assert(len(loopbacks) == 1)
loopback = loopbacks[0]
assert(not loopback.hardware())
assert(loopback.index() > 0)
assert(loopback.mtu() > 0)
assert(loopback.up())
assert(loopback.loopback())
assert(any(address.addr.str() == "127.0.0.1" for address in loopback.addresses()))
assert(NetInterface.kind_str(NetInterface.Kind.wifi) == "wifi")

loopback_route = sihd.net.route.find(sihd.net.IpAddr("127.0.0.1"))
assert(loopback_route.interface_name == loopback.name())
assert(loopback_route.source.str() == "127.0.0.1")
for default_route in sihd.net.route.defaults_ipv4():
    assert(default_route.interface_name != "")

for server in sihd.net.dns.servers(sihd.util.time.ms(500)):
    assert(server.addr.has_ip())
    assert(isinstance(server.interface_name, str))

# failures raise sihd.Error
try:
    sihd.net.route.find(sihd.net.IpAddr())
    assert(False)
except sihd.Error as no_destination:
    assert(no_destination.code == sihd.ErrorCode.invalid_argument)
    assert(str(no_destination) == "route lookup needs a destination address")

monitor = sihd.net.NetMonitor(sihd.util.time.ms(10))
assert(monitor.run())
event = sihd.net.NetMonitorEvent()
assert(event.type == sihd.net.NetMonitorEventType.unknown)
assert(not event.removed)
assert(event.interface_name == "")
assert(event.address.empty())
assert(event.type_str() == "unknown")
