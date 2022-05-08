local util = sihd.util

if util.os.is_run_with_asan == false then
    assert(sihd.dir ~= "")
end

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
print("current thread id: " .. util.thread.id_str())

-- signal
-- util.os.kill(util.os.pid(), 2)

-- backtrace
util.os.backtrace_size = 2
assert(util.os.backtrace(util.os.stdout) == 2)

-- resources
assert(util.os.peak_rss() > 0)
assert(util.os.current_rss() > 0)

-- str

local split = util.str.split("hello world", " ")

assert(#split == 2)
assert(split[1] == "hello")
assert(split[2] == "world")