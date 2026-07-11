local net = sihd.net

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
