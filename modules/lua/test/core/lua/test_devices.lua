local core = sihd.core.Core("core", nil)

local function check_device(dev)
    assert(dev ~= nil)
    assert(type(dev:name()) == "string")
    assert(type(dev:device_state()) == "number")
    assert(type(dev:device_state_str()) == "string")
end

check_device(sihd.core.DevFilter("filter", core))
check_device(sihd.core.DevSampler("sampler", core))
check_device(sihd.core.DevPlayer("player", core))
check_device(sihd.core.DevRecorder("recorder", core))
