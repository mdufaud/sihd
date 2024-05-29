local echo = sihd.util.Process()
echo:add_argv({"echo", "hello world"})

echo:start()
echo:wait_any(0)
assert(echo:has_exited())
assert(echo:return_code(), 0)

-- run

local ls = sihd.util.Process()
ls:add_argv({"ls"})

local ls_size = 0
ls:stdout_to(function(buf, size)
    ls_size = size
end)

ls:run()
ls:stop()

assert(ls:has_exited())
assert(ls:return_code(), 0)
assert(ls_size > 0)

-- stdin

local wc = sihd.util.Process()
wc:add_argv({"wc", "-c"})

wc:stdin_from("toto")

local result = ""
wc:stdout_to(function(buf, size)
    result = buf
end)

wc:start()
wc:stdin_close()
wc:end_process()

assert(result == "4\n")