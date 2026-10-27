import sihd

core = sihd.core.Core("core")
worker_pool = sihd.core.DevWorkerPool("worker_pool", core)
assert(worker_pool.set_conf(threads = [2, 4], queue_size = 8))
