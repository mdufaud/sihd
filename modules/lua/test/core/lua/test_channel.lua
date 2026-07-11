local core = sihd.core.Core("core", nil)
local ch_bool = sihd.core.Channel("ch_bool", "bool", 1, core)

assert(ch_bool:name() == "ch_bool")
assert(ch_bool:write(0, true))
assert(ch_bool:read(0) == true)

local ch_byte = sihd.core.Channel("ch_byte", "byte", 1, core)

assert(ch_byte:write(0, 127))
assert(ch_byte:read(0) == 127)

local ch_long = sihd.core.Channel("ch_long", "long", 1, core)

assert(ch_long:write(0, 9223372036854775800))
assert(ch_long:read(0) == 9223372036854775800)

local ch_float = sihd.core.Channel("ch_float", "float", 1, core)

assert(ch_float:write(0, 1.234))
local val = ch_float:read(0)
assert(val > 1.233 and val < 1.235)

-- remaining integer types: verify numeric coercion across the bridge edges
local ch_ubyte = sihd.core.Channel("ch_ubyte", "ubyte", 1, core)
assert(ch_ubyte:write(0, 255))
assert(ch_ubyte:read(0) == 255)

local ch_short = sihd.core.Channel("ch_short", "short", 1, core)
assert(ch_short:write(0, -32768))
assert(ch_short:read(0) == -32768)

local ch_ushort = sihd.core.Channel("ch_ushort", "ushort", 1, core)
assert(ch_ushort:write(0, 65535))
assert(ch_ushort:read(0) == 65535)

local ch_int = sihd.core.Channel("ch_int", "int", 1, core)
assert(ch_int:write(0, 2147483647))
assert(ch_int:read(0) == 2147483647)

local ch_uint = sihd.core.Channel("ch_uint", "uint", 1, core)
assert(ch_uint:write(0, 4294967295))
assert(ch_uint:read(0) == 4294967295)

local ch_ulong = sihd.core.Channel("ch_ulong", "ulong", 1, core)
assert(ch_ulong:write(0, 4294967296))
assert(ch_ulong:read(0) == 4294967296)

local ch_double = sihd.core.Channel("ch_double", "double", 1, core)
assert(ch_double:write(0, 3.141592653589793))
assert(ch_double:read(0) == 3.141592653589793)

-- accessors
local arr = ch_int:array()
assert(arr ~= nil)
assert(arr:size() == 1)
assert(arr:data_type_str() == "int")

local ts = ch_int:timestamp()
assert(ts ~= nil)
assert(type(ts:nanoseconds()) == "number")
ch_int:set_write_on_change(true)
ch_int:notify()

-- Device channel container roundtrip
local dyn = core:add_channel("dyn", "int", 2)
assert(dyn ~= nil)
assert(dyn:name() == "dyn")
assert(core:find_channel("dyn"):name() == "dyn")
assert(core:get_channel("dyn"):name() == "dyn")
assert(core:find_channel("does_not_exist") == nil)

-- resize a channel's element count
local rz = sihd.core.Channel("rz", "int", 2, nil)
assert(rz:size() == 2)
assert(rz:resize(5))
assert(rz:size() == 5)
assert(rz:reserve(16))
assert(rz:capacity() >= 16)
