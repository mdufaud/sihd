local echo = sihd.util.Process()
echo:add_argv({ "echo", "hello world" })

echo:execute()
echo:wait_any()
assert(echo:has_exited())
assert(echo:return_code(), 0)

-- run

local ls = sihd.util.Process()
ls:add_argv({ "ls" })

local ls_size = 0
ls:stdout_to(function(str)
    print(str)
    ls_size = str.size()
end)

ls:start()
ls:stop()

assert(ls:has_exited())
assert(ls:return_code(), 0)
assert(ls_size > 0)

-- stdin

local wc = sihd.util.Process()
wc:add_argv({ "wc", "-c" })

wc:stdin_from("toto")

local result = ""
wc:stdout_to(function(str)
    print(str)
    result = str
end)

wc:execute()
wc:stdin_close()
wc:terminate()

assert(result == "4\n")
