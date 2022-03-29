sihd.util.log.info(sihd.dir)
assert(sihd.dir ~= "")

before = sihd.util.clock:now()
sihd.util.time.usleep(1)
after = sihd.util.clock:now()
assert(before < after)

assert(sihd.util.time.us(1), 1000)

assert(sihd.util.thread.get_name(), "main")
sihd.util.thread.set_name("toto")
assert(sihd.util.thread.get_name(), "toto")

print("current thread raw id: " .. sihd.util.thread.id())
print("current thread id: " .. sihd.util.thread.id_str())