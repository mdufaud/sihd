import sihd

core = sihd.core.Core("core")
stats = sihd.core.DevStats("stats", core)
assert(stats.set_conf(period = [10, 20]))
