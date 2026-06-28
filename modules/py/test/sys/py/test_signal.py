import signal as py_signal
import sihd

sig = sihd.sys.signal

# test signal name
name = sig.name(py_signal.SIGTERM)
assert(isinstance(name, str))
assert(len(name) > 0)

# test category checks
assert(sig.is_category_termination(py_signal.SIGTERM))
assert(not sig.is_category_stop(py_signal.SIGTERM))

# test handle/unhandle
assert(sig.handle(py_signal.SIGUSR1))

# test last_received - should be 0 since no signal sent
assert(isinstance(sig.last_received(), int))

# test should_stop
assert(not sig.should_stop())
assert(not sig.stop_received())
assert(not sig.termination_received())
assert(not sig.dump_received())

# test reset_all_received
sig.reset_all_received()

# unhandle the signal we handled
assert(sig.unhandle(py_signal.SIGUSR1))

print("signal tests passed")
