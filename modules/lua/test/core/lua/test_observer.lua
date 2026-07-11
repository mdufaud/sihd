-- Observe a service's state transitions through the ServiceController bridge
local core = sihd.core.Core("core", nil)

local ctrl = core:service_ctrl()
assert(ctrl ~= nil)

local states = {}
ctrl:add_observer(function(c)
    states[#states + 1] = c:state()
end)

assert(core:init())
assert(core:start())
assert(core:stop())

ctrl:remove_observers()

-- init/start/stop each drive at least one state transition -> notification
assert(#states >= 3)
