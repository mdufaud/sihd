-- LineReader

local line_reader = sihd.sys.LineReader()

assert(line_reader:open("test/sys/lua/test_files.lua"))

assert(line_reader:read_next())

local line = line_reader:get_read_data()
assert(line ~= nil)

assert(line:str() ~= "")
assert(line:size() > 0)

line_reader:close()
line_reader = nil

-- File

local file = sihd.sys.File()

assert(file:open("test/sys/lua/test_files.lua", "r"))

local array = sihd.util.ArrChar()
array:reserve(4096)
assert(array:size() == 0)

assert(file:read(array))

assert(array:size() > 0)

file:close()

-- Filesystem

assert(sihd.sys.fs.combine({"a", "b", "c"}) == "a/b/c")
assert(sihd.sys.fs.combine("toto", "tata") == "toto/tata")
