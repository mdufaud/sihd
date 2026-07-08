local util = sihd.util

-- 93784005000000 ns = 1 day 2h 3m 4s 5ms
local t = util.Timestamp(93784005000000)

assert(t:get() == 93784005000000)
assert(t:nanoseconds() == 93784005000000)
assert(t:microseconds() == 93784005000)
assert(t:milliseconds() == 93784005)
assert(t:seconds() == 93784)
assert(t:minutes() == 1563)
assert(t:hours() == 26)
assert(t:days() == 1)

-- string accessors return strings
assert(type(t:str()) == "string")
assert(type(t:day_str("%Y-%m-%d")) == "string")
assert(type(t:format("%Y")) == "string")

-- operators
assert(util.Timestamp(1000) == util.Timestamp(1000))
assert(util.Timestamp(1000) < util.Timestamp(2000))
assert(util.Timestamp(1000) <= util.Timestamp(1000))
assert(util.Timestamp(1000) + util.Duration(1000) == util.Timestamp(2000))
assert(util.Timestamp(3000) - util.Timestamp(1000) == util.Duration(2000))
