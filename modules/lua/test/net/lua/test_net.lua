local net = sihd.net
local util = sihd.util

local function must_raise(fn, ...)
    local ok, err = pcall(fn, ...)
    assert(ok == false, "expected an error, got success")
    return err
end

-- connection detection
local interfaces = net.NetInterface.get_all_interfaces()
assert(interfaces ~= nil)
local loopback = nil
for _, netif in pairs(interfaces) do
    if netif:kind() == net.NetInterface.Kind.loopback then
        loopback = netif
    end
end
assert(loopback ~= nil)
assert(not loopback:hardware())
assert(loopback:index() > 0)
assert(loopback:mtu() > 0)
assert(loopback:up())
assert(loopback:loopback())
local has_localhost = false
for _, address in ipairs(loopback:addresses()) do
    if address.addr:str() == "127.0.0.1" then
        has_localhost = true
    end
end
assert(has_localhost)
assert(net.NetInterface.kind_str(net.NetInterface.Kind.tunnel) == "tunnel")

local loopback_route = net.route.find(net.ip_addr("127.0.0.1"))
assert(loopback_route ~= nil)
assert(loopback_route.interface_name == loopback:name())
assert(loopback_route.source:str() == "127.0.0.1")
local defaults = net.route.defaults_ipv4()
assert(defaults ~= nil)
for _, default_route in ipairs(defaults) do
    assert(default_route.interface_name ~= "")
end

local servers = net.dns.servers(util.Duration(util.time.sec(1)))
assert(servers ~= nil)
for _, server in ipairs(servers) do
    assert(server.addr:has_ip())
end

-- failures raise a sihd.Error
local missing = must_raise(net.wifi.link, "sihd-no-such-interface")
assert(missing.code == sihd.ErrorCode.not_found)
assert(missing.message == "no interface named sihd-no-such-interface")
assert(tostring(missing) == "no interface named sihd-no-such-interface")
local no_destination = must_raise(net.route.find, net.IpAddr())
assert(no_destination.code == sihd.ErrorCode.invalid_argument)
assert(no_destination.message == "route lookup needs a destination address")

assert(net.NetMonitor.open(nil) ~= nil)
local monitor = net.NetMonitor.open(util.Duration(util.time.ms(10)))
assert(monitor:run())
local event = net.NetMonitorEvent()
assert(event.type == net.NetMonitorEventType.unknown)
assert(event.removed == false)
assert(event.interface_name == "")
assert(event.address:empty())
assert(event:type_str() == "unknown")

-- IpAddr value type
local addr = net.ip_addr("127.0.0.1", 4300)
assert(addr:is_ipv4())
assert(not addr:is_ipv6())
assert(addr:has_ip())
assert(addr:port() == 4300)
assert(addr:str():find("127.0.0.1") ~= nil)

local addr_noport = net.ip_addr("127.0.0.1")
assert(addr_noport:port() == 0)

-- UDP device loopback through a Core
local core = sihd.core.Core("core", nil)
local sender = net.DeviceUdpSender("sender", core)
local receiver = net.DeviceUdpReceiver("receiver", core)

assert(sender:set_conf({host = "127.0.0.1", port = 4300}))
assert(receiver:set_conf({host = "127.0.0.1", port = 4300}))
assert(receiver:set_poll_timeout(1))
assert(receiver:set_buffer_capacity(1024))

assert(core:init())

local rx = receiver:find_channel("rx")
local tx = sender:find_channel("tx")
assert(rx ~= nil)
assert(tx ~= nil)

assert(core:start())
assert(receiver:is_running())

local waiter = sihd.core.ChannelWaiter(rx)

local payload = "hello udp device"
assert(tx:write_array(sihd.util.ArrChar.new(payload)))

-- prev_wait_for: same thread writes then waits, so a packet that lands before this
-- call must still count (wait_for would ask for a second one and time out)
assert(waiter:prev_wait_for(sihd.util.time.ms(2000), 1))

local out = sihd.util.ArrChar.new("")
out:resize(rx:array():size())
assert(rx:copy_to(out))
assert(out:str() == payload)

assert(core:stop())
assert(not receiver:is_running())
