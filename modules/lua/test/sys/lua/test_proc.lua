local proc = sihd.sys.proc

-- success exit code crosses the bridge
local fut = proc.execute({ "true" })
assert(fut:valid() == true)
assert(fut:get() == 0)
assert(fut:valid() == false)

-- failure exit code
assert(proc.execute({ "false" }):get() ~= 0)

local fut_echo = proc.execute({ "echo", "hi" })
local ready = fut_echo:wait_for(sihd.util.time.sec(2))
assert(type(ready) == "boolean")
assert(ready == true)
assert(fut_echo:get() == 0)