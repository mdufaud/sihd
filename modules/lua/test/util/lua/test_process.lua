-- Test Process with thread-safe callbacks using flush()
local echo = sihd.util.Process()
echo:add_argv({ "echo", "hello world" })

echo:execute()
echo:wait_any(0)
assert(echo:has_exited())
assert(echo:return_code() == 0)

-- Test stdout_to with callback and flush using execute + read_pipes (manual mode)
local ls = sihd.util.Process()
ls:add_argv({ "ls" })

local ls_output = ""
ls:stdout_to(function(str)
    ls_output = ls_output .. str
end)

-- execute() spawns the process
ls:execute()
-- Read pipes until no more data, then wait for process to finish
while ls:read_pipes(100) do end
ls:wait_any(0)
-- Flush the callback queue from the Lua thread (data was queued by reader thread)
ls:flush()

assert(ls:has_exited())
assert(ls:return_code() == 0)
assert(#ls_output > 0, "Expected ls output, got empty string")
print("ls output length: " .. #ls_output)

-- Test stdin with stdout callback using execute + read_pipes
local wc = sihd.util.Process()
wc:add_argv({ "wc", "-c" })

wc:stdin_from("toto")

local wc_result = ""
wc:stdout_to(function(str)
    wc_result = wc_result .. str
end)

wc:execute()
wc:stdin_close()
-- Read pipes manually until process ends
while wc:read_pipes(100) do end
wc:wait_any(0)
-- Flush the callback queue from the Lua thread
wc:flush()

assert(wc:has_exited())
local code = wc:return_code()
assert(code == 0, "wc return code: " .. tostring(code))

-- wc -c on "toto" should return "4\n" or "4" depending on platform
print("wc result: '" .. wc_result .. "'")
assert(wc_result:match("^%s*4%s*$"), "Expected '4', got: '" .. wc_result .. "'")

-- Test stderr callback
local err_proc = sihd.util.Process()
err_proc:add_argv({ "ls", "/nonexistent_path_12345" })

local err_output = ""
err_proc:stderr_to(function(str)
    err_output = err_output .. str
end)

err_proc:execute()
-- Read pipes manually until process ends
while err_proc:read_pipes(100) do end
err_proc:wait_any(0)
err_proc:flush()

assert(err_proc:has_exited())
assert(err_proc:return_code() ~= 0, "Expected non-zero return code for ls on nonexistent path")
assert(#err_output > 0, "Expected stderr output for ls on nonexistent path")
print("stderr output: " .. err_output)

sihd.util.log.info("Process test passed!")
