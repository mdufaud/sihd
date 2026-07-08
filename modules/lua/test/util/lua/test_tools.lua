local util = sihd.util

-- clock
before = util.clock:now()
util.time.usleep(1)
after = util.clock:now()
assert(before < after)

-- time
assert(util.time.us(1) == 1000)

-- thread
util.thread.set_name("toto")
assert(util.thread.name() == "toto")

print("current thread raw id: " .. util.thread.id())
print("current thread id: " .. util.thread.id_str(util.thread.id()))

-- time converters (to nano and back)
assert(util.time.ms(1) == 1000000)
assert(util.time.sec(1) == 1000000000)
assert(util.time.to_us(1000) == 1)
assert(util.time.to_ms(1000000) == 1)
assert(util.time.to_sec(1000000000) == 1)

-- str
assert(util.str.trim("  hello  ") == "hello")
assert(util.str.replace("a-b-c", "-", "_") == "a_b_c")
assert(util.str.starts_with("hello world", "hello", "") == true)
assert(util.str.starts_with("hello world", "world", "") == false)
assert(util.str.ends_with("hello world", "world", "") == true)
assert(util.str.ends_with("hello world", "hello", "") == false)

local split = util.str.split("hello world", " ")

assert(#split == 2)
assert(split[1] == "hello")
assert(split[2] == "world")

-- endian: exactly one is true
assert(util.endian.is_big() ~= util.endian.is_little())