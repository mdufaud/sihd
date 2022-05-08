local core = sihd.core.Core("core", nil)
local ch_bool = sihd.core.Channel("ch_bool", "bool", 1, core)

assert(ch_bool:name() == "ch_bool")
assert(ch_bool:write(0, true))
assert(ch_bool:read(0) == true)