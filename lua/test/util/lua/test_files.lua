-- LineReader

local line_reader = sihd.util.LineReader()

assert(line_reader:open("test/util/lua/test_files.lua"))

assert(line_reader:read_next())

local line = line_reader:get_read_data()
assert(line ~= nil)

assert(line:str() ~= "")
assert(line:size() > 0)

line_reader:close()
line_reader = nil

-- File

local file = sihd.util.File()

assert(file:open("test/util/lua/test_files.lua", "r"))

local array = sihd.util.ArrChar()
array:reserve(4096)
assert(array:size() == 0)

assert(file:read(array))

assert(array:size() > 0)

file:close()

-- Filesystem

assert(sihd.util.fs.combine({"a", "b", "c"}) == "a/b/c")
assert(sihd.util.fs.combine("toto", "tata") == "toto/tata")