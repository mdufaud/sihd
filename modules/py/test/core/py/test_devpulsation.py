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
try:
    core.find_channel("does_not_exist")
    assert(False)
except sihd.Error as missing:
    assert(missing.code == sihd.ErrorCode.not_found)
    assert(str(missing) == "no such channel 'does_not_exist' in 'core'")
waiter = sihd.core.ChannelWaiter(ch_beat)

i = 0
def obs(ch):
    global i
    i = i + 1
    print("beat: " + ch.name())

ch_beat.set_observer(obs)

ch_activate.write(0, True)
ret = waiter.wait_for(sihd.util.time.ms(2000), 5)
ch_activate.write(0, False)
ch_beat.set_observer(None)

print("Total beat: %d" % i)
assert(i >= 5)
assert(ret)

core.stop()

# DevMessage construct + configure
msg = sihd.core.DevMessage("message", core)
assert(msg.set_trigger_mode(True))