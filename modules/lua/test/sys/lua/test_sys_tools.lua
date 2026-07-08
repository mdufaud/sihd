local sys = sihd.sys

if sys.os.is_run_with_asan == false then
    assert(sihd.dir ~= "")
end

-- signal
-- sys.os.kill(sys.os.pid(), 2)
local signame = sys.signal.name(2)
assert(type(signame) == "string")
assert(signame ~= "")

-- backtrace
assert(sys.os.backtrace(sys.os.stdout, 10) == 10)

-- resources
assert(sys.os.peak_rss() > 0)
assert(sys.os.current_rss() > 0)
