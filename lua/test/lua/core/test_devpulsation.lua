local core = sihd.core.Core("core", nil)
local dev = sihd.core.DevPulsation("devpulsation", core)

-- 1000 per second -> 1ms
dev:set_conf({
    frequency = 1000.0
})

core:init()
core:start()

local ch_activate = dev:get_channel("activate")
local ch_beat = dev:get_channel("heartbeat")
local waiter = sihd.core.ChannelWaiter(ch_beat)

ch_activate:write(0, true)

--[[
ch_beat:set_observer(function (ch)
    print(ch:get_name())
end)
]]

local res = waiter:wait_for(sihd.util.time.ms(10), 9)

--ch_beat:set_observer(nil)

assert(res == true)

core:stop()