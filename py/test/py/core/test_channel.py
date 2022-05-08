import sihd

ch_bool = sihd.core.Channel("ch_bool", "bool", 1)

assert(ch_bool.name() == "ch_bool")
assert(ch_bool.write(0, False))
assert(ch_bool.read(0) == False)
assert(ch_bool.write(0, True))
assert(ch_bool.read(0) == True)

ch_char = sihd.core.Channel("ch_char", "char", 1)

assert(ch_char.write(0, "a"))
assert(ch_char.read(0) == "a")

ch_byte = sihd.core.Channel("ch_byte", "byte", 1)

assert(ch_byte.write(0, -1))
assert(ch_byte.read(0) == -1)
assert(ch_byte.write(0, 128))
assert(ch_byte.read(0) == -128)

ch_int = sihd.core.Channel("ch_int", "ulong", 3)

assert(ch_int.write(0, 1))
assert(ch_int.write(1, 2))
assert(ch_int.write(2, 3))

assert(ch_int.read(0) == 1)
assert(ch_int.read(1) == 2)
assert(ch_int.read(2) == 3)

ch_float = sihd.core.Channel("ch_float", "float", 1)

assert(ch_float.write(0, 1.234))
f = ch_float.read(0)
assert(f < 1.235 and f > 1.233)

nobs = 0
observed_channel = None
def channel_obs(channel):
    print(channel.name() + " notified")
    global nobs
    global observed_channel
    observed_channel = channel
    nobs += 1

ch_bool.set_observer(channel_obs)
ch_bool.write(0, False)
ch_bool.write(0, False)
ch_bool.set_observer(None)

assert(observed_channel == ch_bool)
assert(nobs == 1)