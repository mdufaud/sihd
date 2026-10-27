import sihd

core = sihd.core.Core("core")
worker = sihd.core.DevWorker("worker", core)
assert(worker.set_conf(queue_size = [8, 16]))
