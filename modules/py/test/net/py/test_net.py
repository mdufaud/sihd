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
