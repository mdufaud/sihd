local util = sihd.util

local d = util.Duration(93784005000000)

assert(d:get() == 93784005000000)
assert(d:nanoseconds() == 93784005000000)
assert(d:microseconds() == 93784005000)
assert(d:milliseconds() == 93784005)
assert(d:seconds() == 93784)
assert(d:minutes() == 1563)
assert(d:hours() == 26)
assert(d:days() == 1)

assert(type(d:str()) == "string")

-- operators
assert(util.Duration(1000) == util.Duration(1000))
assert(util.Duration(1000) < util.Duration(2000))
assert(util.Duration(1000) <= util.Duration(1000))
assert(util.Duration(1000) + util.Duration(1000) == util.Duration(2000))
assert(util.Duration(3000) - util.Duration(1000) == util.Duration(2000))
