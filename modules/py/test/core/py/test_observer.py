import sihd

# Observe a service's state transitions through the ServiceController bridge
core = sihd.core.Core("core")

ctrl = core.service_ctrl()
assert ctrl is not None

states = []
ctrl.add_observer(lambda c: states.append(c.state()))

assert core.init()
assert core.start()
assert core.stop()

ctrl.remove_observers()

# init/start/stop each drive at least one state transition -> notification
assert len(states) >= 3

print("observer tests passed")
