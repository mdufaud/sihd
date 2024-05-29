local core = sihd.core.Core("core", nil)
local ch_bool = sihd.core.Channel("ch_bool", "bool", 1, core)

assert(ch_bool:name() == "ch_bool")
assert(ch_bool:write(0, true))
assert(ch_bool:read(0) == true)

local ch_byte = sihd.core.Channel("ch_byte", "byte", 1, core)

assert(ch_byte:write(0, 128))
assert(ch_byte:read(0) == -128)

local ch_long = sihd.core.Channel("ch_long", "long", 1, core)

assert(ch_long:write(0, 9223372036854775800))
assert(ch_long:read(0) == 9223372036854775800)

local ch_float = sihd.core.Channel("ch_float", "float", 1, core)

assert(ch_float:write(0, 1.234))
local val = ch_float:read(0)
assert(val > 1.233 and val < 1.235)