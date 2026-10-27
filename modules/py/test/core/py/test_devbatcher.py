import sihd

core = sihd.core.Core("core")
batcher = sihd.core.DevBatcher("batcher", core)
assert(batcher.set_conf(samples = [4, 8], period = [10, 20]))
