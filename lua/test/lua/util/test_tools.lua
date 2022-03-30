local util = sihd.util

-- log
if util.os.is_run_with_asan == false then
    util.log.info(sihd.dir)
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
assert(util.thread.get_name(), "main")
util.thread.set_name("toto")
assert(util.thread.get_name(), "toto")

print("current thread raw id: " .. util.thread.id())
print("current thread id: " .. util.thread.id_str())

-- signal
--util.os.kill(util.os.pid(), 2)

-- backtrace
util.os.backtrace_size = 2
util.os.backtrace(util.os.stdout)

-- resources
print(util.os.get_peak_rss())
print(util.os.get_current_rss())