local zip = sihd.zip

-- create a source file on disk to add into the archive
local content = "Hello from lua zip binding"
local src_path = os.tmpname()
local f = assert(io.open(src_path, "w"))
f:write(content)
f:close()

local archive = os.tmpname()
os.remove(archive) -- start from a fresh archive path

-- write an archive with a directory and a file taken from the filesystem
local writer = zip.ZipFile()
assert(writer:open(archive))
assert(writer:is_open())
assert(writer:comment_archive("lua archive"))
assert(writer:add_dir("data"))
assert(writer:add_file_from_fs("data/hello.txt", src_path))
assert(writer:count_entries() == 2)
assert(writer:close())

-- namespace helper: list entries of the saved archive
local entries = zip.list_entries(archive)
assert(type(entries) == "table")
local names = {}
for _, name in ipairs(entries) do
    names[name] = true
end
assert(names["data/"] ~= nil)
assert(names["data/hello.txt"] ~= nil)

-- reopen read-only and read the file content back
local reader = zip.ZipFile()
assert(reader:open(archive, true))
assert(reader:is_open())
assert(reader:archive_comment() == "lua archive")

assert(reader:load_entry("data/"))
assert(reader:is_entry_directory())

assert(reader:load_entry("data/hello.txt"))
assert(reader:is_entry_loaded())
assert(not reader:is_entry_directory())
assert(reader:entry_name() == "data/hello.txt")
assert(reader:read_entry() == #content)
local data = reader:get_read_data()
assert(data:str() == content)
reader:close()

-- namespace unzip: extract archive to a directory
local dest = os.tmpname()
os.remove(dest)
assert(zip.unzip(archive, dest))
local extracted = assert(io.open(dest .. "/data/hello.txt", "r"))
assert(extracted:read("*a") == content)
extracted:close()

os.remove(src_path)
os.remove(archive)
