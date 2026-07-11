local csv = sihd.csv

-- from_string: parse content into rows of string cells
local rows = csv.from_string("a,b,c\n1,2,3\n4,5,6\n")
assert(type(rows) == "table")
assert(#rows == 3)
assert(rows[1][1] == "a")
assert(rows[1][3] == "c")
assert(rows[2][2] == "2")
assert(rows[3][3] == "6")

-- from_string: remove_header skips first row
local no_header = csv.from_string("h1,h2\n10,20\n", true)
assert(#no_header == 1)
assert(no_header[1][1] == "10")

-- from_string: custom delimiter
local semi = csv.from_string("x;y;z\n", false, ";")
assert(semi[1][2] == "y")

-- escape_str: encloses and doubles quotes
assert(csv.escape_str('hello "world"') == '"hello ""world"""')

-- write then read: whole-file round-trip
local path = os.tmpname()
assert(csv.write(path, {{"name", "port"}, {"dev", "8080"}}))
local back = csv.read(path)
assert(type(back) == "table")
assert(#back == 2)
assert(back[1][1] == "name")
assert(back[2][1] == "dev")
assert(back[2][2] == "8080")

-- read: nonexistent file -> nil
assert(csv.read("/nonexistent/path/file.csv") == nil)

-- CsvWriter streaming
local writer = csv.CsvWriter()
local wpath = os.tmpname()
assert(writer:open(wpath))
assert(writer:is_open())
writer:write_commentary("a comment")
writer:write_row({"col1", "col2"})
writer:write_row({"v1", "v2"})
assert(writer:current_row() >= 2)
writer:close()

-- CsvReader streaming reads back the rows (comment skipped)
local reader = csv.CsvReader()
assert(reader:open(wpath))
assert(reader:is_open())
assert(reader:read_next())
local cols = reader:columns()
assert(cols[1] == "col1")
assert(cols[2] == "col2")
assert(reader:read_next())
cols = reader:columns()
assert(cols[1] == "v1")
assert(cols[2] == "v2")
assert(reader:read_next() == false)
reader:close()

os.remove(path)
os.remove(wpath)
