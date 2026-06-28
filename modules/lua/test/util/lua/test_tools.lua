local util = sihd.util

-- clock
before = util.clock:now()
util.time.usleep(1)
after = util.clock:now()
assert(before < after)

-- time
assert(util.time.us(1), 1000)

-- thread
assert(util.thread.name(), "main")
util.thread.set_name("toto")
assert(util.thread.name(), "toto")

print("current thread raw id: " .. util.thread.id())
print("current thread id: " .. util.thread.id_str(util.thread.id()))

-- str

local split = util.str.split("hello world", " ")

assert(#split == 2)
assert(split[1] == "hello")
assert(split[2] == "world")