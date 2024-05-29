local core = sihd.core.Core("core", nil)
local dev = sihd.core.DevPulsation("devpulsation", core)

-- 500 per second -> 2ms
dev:set_conf({
    frequency = 500.0
})

assert(core:init())
assert(core:start())

local ch_activate = dev:get_channel("activate")
local ch_beat = dev:get_channel("heartbeat")
local waiter = sihd.core.ChannelWaiter(ch_beat)

local i = 0
ch_beat:set_observer(function (ch)
    i = i + 1
end)

ch_activate:write(0, true)
local ret = waiter:wait_for(sihd.util.Timestamp(sihd.util.time.ms(50)), 5)
ch_activate:write(0, false)

ch_beat:set_observer(nil)

print("Total beat: " .. i)
assert(i >= 5)
assert(ret == true)

core:stop()