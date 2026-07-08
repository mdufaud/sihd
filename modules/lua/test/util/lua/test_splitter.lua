local util = sihd.util

local sp = util.Splitter()
sp:set_delimiter(",")

local tokens = sp:split("a,b,c")
assert(#tokens == 3)
assert(tokens[1] == "a")
assert(tokens[2] == "b")
assert(tokens[3] == "c")

assert(sp:count_tokens("a,b,c") == 3)

-- delimiter by spaces
sp:set_delimiter_spaces()
local words = sp:split("hello   world")
assert(#words == 2)
assert(words[1] == "hello")
assert(words[2] == "world")

-- toggle setters are reachable
sp:set_empty_delimitations(true)
sp:set_open_escape_sequences("\"'")
sp:set_escape_sequences_all()
