import sihd

os_mod = sihd.sys.os

# test pid
pid = os_mod.pid()
assert(isinstance(pid, int))
assert(pid > 0)

# test peak_rss
peak = os_mod.peak_rss()
assert(isinstance(peak, int))
assert(peak > 0)

# test current_rss
current = os_mod.current_rss()
assert(isinstance(current, int))
assert(current > 0)

# test is_root
is_root = os_mod.is_root()
assert(isinstance(is_root, bool))

# test max_fds
max_fds = os_mod.max_fds()
assert(isinstance(max_fds, int))
assert(max_fds > 0)

# test boot_time
boot = os_mod.boot_time()
assert(isinstance(boot, int))
assert(boot > 0)

# test exists_in_path
assert(os_mod.exists_in_path("ls"))
assert(not os_mod.exists_in_path("nonexistent_binary_xyz_12345"))

# test last_error_str
err = os_mod.last_error_str()
assert(isinstance(err, str))

# test is_run_by_valgrind
assert(isinstance(os_mod.is_run_by_valgrind(), bool))

# test is_run_by_debugger
assert(isinstance(os_mod.is_run_by_debugger(), bool))

print("os tests passed")
