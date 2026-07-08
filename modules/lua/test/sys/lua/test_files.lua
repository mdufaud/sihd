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

local fs = sihd.sys.fs

assert(fs.combine({"a", "b", "c"}) == "a/b/c")
assert(fs.combine("toto", "tata") == "toto/tata")

-- path queries on the running test file
local existing = "test/sys/lua/test_files.lua"
assert(fs.exists(existing) == true)
assert(fs.is_file(existing) == true)
assert(fs.is_dir(existing) == false)
assert(fs.filename(existing) == "test_files.lua")
assert(fs.extension(existing) == "lua")
assert(fs.parent(existing) == "test/sys/lua")
assert(fs.is_absolute(existing) == false)
assert(fs.is_absolute("/tmp") == true)
assert(type(fs.normalize(existing)) == "string")
assert(type(fs.cwd()) == "string")

-- temp dir helpers cross the bridge as strings
assert(type(fs.tmp_path()) == "string")
assert(fs.is_absolute(fs.tmp_path()) == true)

-- work inside a real temporary directory, cleaned up at the end
local scratch = fs.make_tmp_directory("sihd_lua_")
assert(type(scratch) == "string")
assert(scratch ~= "")
assert(fs.is_dir(scratch) == true)

-- directory create / children / remove roundtrip
local subdir = fs.combine(scratch, "sub")
assert(fs.make_directory(subdir, 493) == true)
assert(fs.is_dir(subdir) == true)
local kids = fs.children(subdir)
assert(type(kids) == "table")
assert(#kids == 0)
assert(fs.remove_directory(subdir) == true)
assert(fs.exists(subdir) == false)

-- File write -> flush -> read roundtrip + lock
local tmpfile = fs.combine(scratch, "lua_file_scratch.txt")
local wf = sihd.sys.File()
assert(wf:open(tmpfile, "w"))
wf:lock()
assert(wf:write("hello file") == 10)
assert(wf:flush())
wf:unlock()
assert(type(wf:trylock()) == "boolean")
wf:unlock()
wf:close()

local rf = sihd.sys.File()
assert(rf:open(tmpfile, "r"))
local buf = sihd.util.ArrChar()
buf:reserve(64)
assert(rf:read(buf))
assert(buf:str() == "hello file")
rf:close()

assert(fs.remove_file(tmpfile) == true)
assert(fs.remove_directory(scratch) == true)
assert(fs.exists(scratch) == false)
