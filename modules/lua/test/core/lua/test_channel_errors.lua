local util = sihd.util
local log = util.log

local function must_raise(fn, what)
    local ok = pcall(fn)
    assert(ok == false, what .. ": expected an error, got success")
end

local core = sihd.core.Core("core", nil)

log.info("Channel:set_observer rejects non-function observers")
do
    local ch = sihd.core.Channel("obs_bad", "int", 1, core)
    must_raise(function() ch:set_observer(42) end, "set_observer(number)")
    must_raise(function() ch:set_observer("nope") end, "set_observer(string)")
    -- nil with no registered observer is a safe no-op and must not raise
    local ok = pcall(function() ch:set_observer(nil) end)
    assert(ok, "set_observer(nil) with no observer must not raise")
end

log.info("Channel notify re-enters the GIL on the same thread")
do
    local ch = sihd.core.Channel("obs_reentrant", "int", 1, core)
    local seen = 0
    ch:set_observer(function()
        seen = seen + 1
    end)
    assert(ch:write(0, 7))
    ch:notify()
    assert(seen >= 1, "observer must have run synchronously under a reentrant GIL")
    -- removing the observer stops further callbacks
    ch:set_observer(nil)
    local before = seen
    ch:notify()
    assert(seen == before, "removed observer must not run again")
end

log.info("Device:set_conf rejects malformed configuration tables")
do
    must_raise(function() core:set_conf(42) end, "set_conf(number)")
    must_raise(function() core:set_conf("nope") end, "set_conf(string)")
    must_raise(function() core:set_conf({ [1] = "x" }) end, "set_conf(non-string key)")
    -- a string key with no matching configuration callback raises
    must_raise(function() core:set_conf({ no_such_conf_key = true }) end, "set_conf(unknown key)")
end

log.info("all core bad-path assertions passed")
return true
