import os
import sihd

ProcessInfo = sihd.sys.ProcessInfo

# test ProcessInfo with current PID
my_pid = os.getpid()
info = ProcessInfo(my_pid)

assert(info.is_alive())
assert(info.pid() == my_pid)

# test name
name = info.name()
assert(isinstance(name, str))
assert(len(name) > 0)

# test exe_path
exe = info.exe_path()
assert(isinstance(exe, str))
assert(len(exe) > 0)

# test cmd_line
cmd = info.cmd_line()
assert(isinstance(cmd, list))
assert(len(cmd) > 0)

# test cwd
cwd = info.cwd()
assert(isinstance(cwd, str))

# test env
env = info.env()
assert(isinstance(env, list))

# test creation_time
creation = info.creation_time()
assert(isinstance(creation, int))
assert(creation > 0)

# test get_all_process_from_name with init or systemd
# just verify it returns a list and doesn't crash
results = ProcessInfo.get_all_process_from_name(".*")
assert(isinstance(results, list))

print("process_info tests passed")
