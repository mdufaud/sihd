from time import sleep
import sihd

sched = sihd.util.Scheduler("scheduler")

i = 0
def fun():
    global i
    i += 1

assert(sched.set_conf(
    no_delay = False,
))

sched.add_task(fun, run_in=0, reschedule_time=sihd.util.time.ms(10))

print("thread start")
assert(sched.start())
sleep(0.2)

print("thread stop")
assert(sched.stop())
sleep(0.1)

assert(i > 10)