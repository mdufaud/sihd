import threading

import sihd

core = sihd.core.Core("core")
dev = sihd.core.DevPulsation("devpulsation", core)
assert(dev.set_conf(frequency = 1000.0))
assert(core.init())
assert(core.start())

ch_activate = dev.get_channel("activate")
ch_beat = dev.get_channel("heartbeat")
ch_activate.write(0, True)

def on_beat(ch):
    pass

for _ in range(2000):
    ch_beat.set_observer(on_beat)
    ch_beat.set_observer(None)

ch_activate.write(0, False)
assert(core.stop())

service = sihd.core.Core("service")
assert(service.init())
ctrl = service.service_ctrl()
running = True

def toggle():
    while running:
        service.start()
        service.stop()

toggler = threading.Thread(target = toggle)
toggler.start()
for _ in range(2000):
    ctrl.add_observer(lambda c: None)
    ctrl.remove_observers()
running = False
toggler.join()

print("observer threads tests passed")
