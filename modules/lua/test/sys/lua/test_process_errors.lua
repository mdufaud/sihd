local util = sihd.util
local log = util.log

local function must_raise(fn, what)
    local ok = pcall(fn)
    assert(ok == false, what .. ": expected an error, got success")
end

log.info("Process:add_argv rejects non-string, non-table arguments")
do
    local proc = sihd.sys.Process()
    must_raise(function() proc:add_argv(42) end, "add_argv(number)")
    must_raise(function() proc:add_argv(true) end, "add_argv(bool)")
    -- the valid forms must not raise
    proc:add_argv("echo")
    proc:add_argv({ "hello", "world" })
end

log.info("Process:stdout_to / stderr_to reject non-function callbacks")
do
    local proc = sihd.sys.Process()
    must_raise(function() proc:stdout_to(42) end, "stdout_to(number)")
    must_raise(function() proc:stdout_to("nope") end, "stdout_to(string)")
    must_raise(function() proc:stderr_to(42) end, "stderr_to(number)")
    must_raise(function() proc:stderr_to("nope") end, "stderr_to(string)")
end

log.info("Executing a nonexistent binary fails cleanly")
do
    local proc = sihd.sys.Process()
    proc:add_argv({ "no_such_binary_sihd_12345" })
    assert(proc:execute() == false, "executing a nonexistent binary must return false")
end

log.info("all process bad-path assertions passed")
return true
