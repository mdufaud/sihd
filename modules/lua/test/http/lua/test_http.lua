local http = sihd.http

local nav = http.Navigator()
nav:clear_proxy()
nav:set_timeout(5)

-- GET
local resp = nav:get("localhost:3011/api/hello")
assert(resp ~= nil)
assert(resp.status == 200)
assert(resp.content == "navigator-ok")
assert(type(resp.headers) == "table")
assert(type(resp.cookies) == "table")
assert(type(resp.redirect_history) == "table")

-- POST echo
local echoed = nav:post("localhost:3011/api/echo", "hello lua")
assert(echoed ~= nil)
assert(echoed.status == 200)
assert(echoed.content == "hello lua")

-- unknown route yields a response (404), not nil
local missing = nav:get("localhost:3011/api/nope")
assert(missing ~= nil)
assert(missing.status == 404)
