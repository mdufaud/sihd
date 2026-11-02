local sys = sihd.sys

-- emscripten has no executable path
if sys.os.is_run_with_asan == false and sys.os.is_emscripten == false then
    assert(sihd.dir ~= "")
end

-- signal
-- sys.os.kill(sys.os.pid(), 2)
local signame = sys.signal.name(2)
assert(type(signame) == "string")
assert(signame ~= "")

-- backtrace
-- arm32 unwinding stops at the first frame without unwind tables, the C of liblua; emscripten has none
local frames = sys.os.backtrace(sys.os.stdout, 10)
assert(frames >= 0 and frames <= 10)

-- resources
if sys.os.is_emscripten == false then
    assert(sys.os.peak_rss() > 0)
    assert(sys.os.current_rss() > 0)
end

-- user
assert(type(sys.user.is_root()) == "boolean")
if sys.os.is_emscripten == false then
    assert(#sys.user.name() > 0)
end
