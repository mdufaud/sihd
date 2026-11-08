local zlib = sihd.zlib

local deflater = zlib.Deflater()
local inflater = zlib.Inflater(4096)
local text = string.rep("lua squeezes bytes. ", 50)
local chunk = deflater:deflate_stream(text)
assert(#chunk < #text)
assert(inflater:inflate_stream(chunk) == text)
assert(inflater:inflate_stream(deflater:deflate_stream("second chunk")) == "second chunk")

local gzip = zlib.Deflater(zlib.Deflater.Format.gzip, 9)
local message = gzip:deflate_message("gzip from lua")
assert(message:byte(1) == 0x1f)
assert(message:byte(2) == 0x8b)
assert(zlib.Inflater(4096):inflate_message(message) == "gzip from lua")
assert(gzip:output_bound(100) > 100)

local bad_level_ok, bad_level = pcall(zlib.Deflater, zlib.Deflater.Format.zlib, 10)
assert(not bad_level_ok)
assert(bad_level.code == sihd.ErrorCode.invalid_argument)
assert(bad_level.message == "zlib: compression level 10 is outside 0..9")

local default_level = zlib.Deflater(zlib.Deflater.Format.gzip)
assert(zlib.Inflater(4096):inflate_message(default_level:deflate_message("default level")) == "default level")

local cast_error = "The lua object can't be cast to desired type"
local bad_arguments = {
    {"argument 1: " .. cast_error, zlib.Deflater, 42},
    {"argument 2: " .. cast_error, zlib.Deflater, zlib.Deflater.Format.zlib, "x"},
    {"argument 1: " .. cast_error, zlib.Inflater},
    {"argument 1: The native integer can't fit inside a lua integer", zlib.Inflater, -1},
}
for _, call in ipairs(bad_arguments) do
    local ok, error = pcall(table.unpack(call, 2))
    assert(not ok)
    assert(error.code == sihd.ErrorCode.invalid_argument)
    assert(error.message == call[1])
end

local truncated_ok, truncated = pcall(function()
    return zlib.Inflater(4096):inflate_message(message:sub(1, #message - 3))
end)
assert(not truncated_ok)
assert(truncated.code == sihd.ErrorCode.invalid_argument)
assert(truncated.message == "zlib inflate: truncated message")

local payload = "a payload compressed by DevDeflate then restored by DevInflate"
local core = sihd.core.Core("core", nil)
core:add_channel("source", "byte", #payload)
local deflate = zlib.DevDeflate("deflate", core)
local inflate = zlib.DevInflate("inflate", core)
assert(deflate:set_mode("message"))
assert(deflate:set_level(9))
assert(deflate:set_format("gzip"))
assert(inflate:set_mode("message"))
assert(inflate:set_output_capacity(1024))
assert(not deflate:set_mode("finish"))
assert(deflate:add_link("source", "..source"))
assert(inflate:add_link("compressed", "..deflate.source_out"))

assert(core:init())
assert(core:start())

local source = core:find_channel("source")
assert(source:write_array(sihd.util.ArrChar.new(payload)))

local restored = inflate:find_channel("compressed_out")
local out = sihd.util.ArrChar.new("")
out:resize(restored:array():size())
assert(restored:copy_to(out))
assert(out:str() == payload)

assert(core:stop())
assert(core:reset())
