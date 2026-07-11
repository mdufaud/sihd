local json = sihd.json

-- decode: scalars
assert(json.decode("true") == true)
assert(json.decode("false") == false)
assert(json.decode("42") == 42)
assert(json.decode("-7") == -7)
assert(json.decode("3.5") == 3.5)
assert(json.decode('"hello"') == "hello")
assert(json.decode("null") == nil)

-- decode: object -> table
local obj = json.decode('{"name":"sihd","count":3,"ok":true,"ratio":0.5}')
assert(type(obj) == "table")
assert(obj.name == "sihd")
assert(obj.count == 3)
assert(obj.ok == true)
assert(obj.ratio == 0.5)

-- decode: array -> 1-based table
local arr = json.decode("[10,20,30]")
assert(type(arr) == "table")
assert(#arr == 3)
assert(arr[1] == 10)
assert(arr[2] == 20)
assert(arr[3] == 30)

-- decode: nested
local nested = json.decode('{"list":[1,2,{"deep":"yes"}],"empty":{}}')
assert(nested.list[1] == 1)
assert(nested.list[3].deep == "yes")
assert(type(nested.empty) == "table")

-- decode: invalid -> nil
assert(json.decode("{not valid") == nil)

-- encode: scalars
assert(json.encode(true) == "true")
assert(json.encode(42) == "42")
assert(json.encode("hi") == '"hi"')
assert(json.encode(nil) == "null")

-- encode: array (1..N integer keys)
assert(json.encode({1, 2, 3}) == "[1,2,3]")

-- encode: object
local encoded_obj = json.encode({key = "value"})
assert(encoded_obj == '{"key":"value"}')

-- encode: empty table -> object (documented behaviour)
assert(json.encode({}) == "{}")

-- round-trip: decode(encode(x)) preserves data
local original = {name = "device", ports = {8080, 8081}, active = true}
local roundtrip = json.decode(json.encode(original))
assert(roundtrip.name == "device")
assert(roundtrip.ports[1] == 8080)
assert(roundtrip.ports[2] == 8081)
assert(roundtrip.active == true)

-- encode with indent produces multi-line output
local pretty = json.encode({a = 1}, 2)
assert(pretty:find("\n") ~= nil)

-- parse/dump aliases
assert(json.parse("[1]")[1] == 1)
assert(json.dump({1}) == "[1]")
