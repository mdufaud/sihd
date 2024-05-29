import sihd

core = sihd.core.Core("core")
dev = sihd.core.DevPulsation("devpulsation", core)

assert(dev.set_conf(
    frequency = [1.0, 2.0, 3.0, 500.0]
))

assert(core.init())
assert(core.start())

ch_activate = dev.get_channel("activate")
ch_beat = dev.get_channel("heartbeat")
waiter = sihd.core.ChannelWaiter(ch_beat)

i = 0
def obs(ch):
    global i
    i = i + 1
    print("beat: " + ch.name())

ch_beat.set_observer(obs)

ch_activate.write(0, True)
ret = waiter.wait_for(sihd.util.Timestamp(sihd.util.time.ms(50)), 5)
ch_activate.write(0, False)
ch_beat.set_observer(None)

print("Total beat: %d" % i)
assert(i >= 5)
assert(ret)

core.stop()