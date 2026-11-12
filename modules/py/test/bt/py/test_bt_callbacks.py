import sys

import sihd

bt = sihd.bt

backend_reads = []
backend_writes = []
unraisable = []
sys.unraisablehook = lambda hook: unraisable.append(type(hook.exc_value).__name__)


def read():
    backend_reads.append(1)
    return b"\x01\x02\x03"


def write(value):
    backend_writes.append(value)
    return value == b"\x0a\x0b"


working = bt.LocalCharacteristic()
working.on_read = read
working.on_write = write

raising = bt.LocalCharacteristic()
raising.on_read = lambda: 1 / 0
raising.on_write = lambda value: 1 / 0

mistyped = bt.LocalCharacteristic()
mistyped.on_read = lambda: "text"
mistyped.on_write = lambda value: "text"
