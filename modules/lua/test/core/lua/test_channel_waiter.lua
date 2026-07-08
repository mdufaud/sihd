local core = sihd.core.Core("core", nil)
local ch = sihd.core.Channel("ch", "int", 1, core)

-- constructor observes the channel directly
local waiter = sihd.core.ChannelWaiter(ch)
assert(waiter:notifications() == 0)

-- each successful write notifies observers -> handler bumps the count
ch:write(0, 42)
assert(waiter:notifications() == 1)
ch:write(0, 43)
assert(waiter:notifications() == 2)

-- clear stops observing and resets the count; further writes are not seen
waiter:clear()
assert(waiter:notifications() == 0)
ch:write(0, 44)
assert(waiter:notifications() == 0)

local waiter2 = sihd.core.ChannelWaiter(ch)
local ns = sihd.util.time.ms(1)
assert(waiter2:wait_for(ns, 1) == false, "raw-ns wait_for should time out to false")
assert(waiter2:wait_for(sihd.util.Duration(ns), 1) == false, "userdata wait_for should time out to false")
