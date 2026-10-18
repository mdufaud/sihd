local core = sihd.core.Core("core", nil)
local dev = sihd.core.DevPulsation("devpulsation", core)

dev:set_conf({
    frequency = 1000.0
})

assert(core:init())
assert(core:start())

local ch_activate = dev:get_channel("activate")
local ch_beat = dev:get_channel("heartbeat")
ch_activate:write(0, true)

local function on_beat(ch)
end

for _ = 1, 2000 do
    ch_beat:set_observer(on_beat)
    ch_beat:set_observer(nil)
end

ch_activate:write(0, false)
assert(core:stop())

local ctrl = service:service_ctrl()
for _ = 1, 2000 do
    ctrl:add_observer(function(c) end)
    ctrl:remove_observers()
end
