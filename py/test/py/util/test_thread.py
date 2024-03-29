from time import sleep
import sihd

sched = sihd.util.Scheduler("scheduler")

i = 0
def fun():
    global i
    i += 1

assert(sched.set_conf(
    as_fast_as_possible = False,
))

sched.add_task(fun, 0, sihd.util.time.ms(10))

print("thread start")
assert(sched.start())
sleep(0.1)

print("thread stop")
assert(sched.stop())
sleep(0.1)

assert(i > 10)