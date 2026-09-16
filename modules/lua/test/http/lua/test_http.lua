local http = sihd.http

local nav = http.Navigator()
nav:clear_proxy()
nav:set_timeout(sihd.util.time.sec(5))

-- GET
local resp = nav:get("localhost:3011/api/hello")
assert(resp ~= nil)
assert(resp.status == 200)
assert(resp.content == "navigator-ok")
assert(type(resp.headers) == "table")
assert(resp.headers["content-type"] == "text/plain; charset=utf-8")
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

-- Navigator: sequential requests on one connection pool
local reuse = http.Navigator()
reuse:clear_proxy()
reuse:set_timeout(sihd.util.time.sec(5))
local r1 = reuse:get("localhost:3011/api/hello")
assert(r1 ~= nil)
assert(r1.status == 200)
assert(r1.content == "navigator-ok")
assert(reuse:new_connection_count() >= 1)

local r2 = reuse:get("localhost:3011/api/hello")
assert(r2 ~= nil)
assert(r2.content == "navigator-ok")
assert(reuse:new_connection_count() == 0)

local r3 = reuse:post("localhost:3011/api/echo", "hello navigator")
assert(r3 ~= nil)
assert(r3.content == "hello navigator")
assert(reuse:new_connection_count() == 0)

-- CORS is on by default: a preflight carries origin and access-control-request-method
local preflight = http.RequestOptions()
preflight:set_header("Origin", "https://app.com")
preflight:set_header("Access-Control-Request-Method", "POST")
local pre = http.options("localhost:3011/api/echo", preflight)
assert(pre ~= nil)
assert(pre.status == 204)
assert(pre.headers["access-control-allow-methods"] == "GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS")

-- stateless requests, one connection per call
local s1 = http.get("localhost:3011/api/hello")
assert(s1 ~= nil)
assert(s1.status == 200)
assert(s1.content == "navigator-ok")

local s2 = http.post("localhost:3011/api/echo", "hello stateless")
assert(s2 ~= nil)
assert(s2.content == "hello stateless")

-- tables are rejected as request bodies instead of being stringified
local ok, err = pcall(function() http.post("localhost:3011/api/echo", {}) end)
assert(not ok)
assert(tostring(err):find("body must be a string") ~= nil)

-- stateless helpers take per request options
local opt = http.RequestOptions()
assert(opt.ssl_verify_peer)
assert(opt.ssl_verify_host)
opt.verbose = false
opt:set_header("X-Test", "1")
opt:set_parameter("q", "1")
opt.timeout = sihd.util.time.sec(5)
local with_options = http.get("localhost:3011/api/hello", opt)
assert(with_options ~= nil)
assert(with_options.status == 200)
assert(with_options.content == "navigator-ok")

-- methods missing from the stateless helpers
local patched = http.patch("localhost:3011/api/echo", "hello patch")
assert(patched ~= nil)
assert(patched.status == 200)
assert(patched.content == "hello patch")

local deleted = http.delete("localhost:3011/api/nope")
assert(deleted ~= nil)
assert(deleted.status == 404)

-- multipart, through the navigator and through the options
local form = http.Multipart()
form:add_field("field", "multipart-value")
assert(not form:empty())
assert(form:value("field") == "multipart-value")

local uploaded = nav:post_multipart("localhost:3011/api/upload", form)
assert(uploaded ~= nil)
assert(uploaded.status == 200)
assert(uploaded.content == "multipart-value")

local with_form = http.RequestOptions()
with_form:set_multipart(form)
local sent = http.post("localhost:3011/api/upload", "", with_form)
assert(sent ~= nil)
assert(sent.status == 200)
assert(sent.content == "multipart-value")

-- a plain OPTIONS reaches the routes: known path, method without a route
local plain = http.options("localhost:3011/api/echo", opt)
assert(plain ~= nil)
assert(plain.status == 405)
assert(plain.headers["allow"] == "POST, PATCH")

-- an OPTIONS route is answered with its body
local handled = http.options("localhost:3011/api/hello", opt)
assert(handled ~= nil)
assert(handled.status == 200)
assert(handled.content == "options-ok")

-- unknown path still yields 404
local nowhere = http.options("localhost:3011/api/nope", opt)
assert(nowhere ~= nil)
assert(nowhere.status == 404)

-- Navigator:last_error is the reason of the last failed request, cleared by the next one
assert(nav:last_error() == "")
local failed = nav:get("localhost:19999/api/hello")
assert(failed == nil)
assert(#nav:last_error() > 0)

local recovered = nav:get("localhost:3011/api/hello")
assert(recovered ~= nil)
assert(recovered.status == 200)
assert(nav:last_error() == "")

-- an unknown proxy type is refused instead of silently defaulting
local proxy_ok = pcall(function()
    local bad = http.RequestOptions()
    bad:set_proxy("localhost:1", "socks9")
end)
assert(not proxy_ok)

-- download writes the response body to disk, put_file streams a file as the body
local download_path = "/tmp/sihd-lua-download.txt"
local downloaded = nav:download("localhost:3011/api/hello", download_path)
assert(downloaded ~= nil)
assert(downloaded.status == 200)
local f = io.open(download_path, "r")
assert(f ~= nil)
assert(f:read("*a") == "navigator-ok")
f:close()

local put_path = "/tmp/sihd-lua-put.txt"
local pf = io.open(put_path, "w")
assert(pf ~= nil)
pf:write("lua put file")
pf:close()
local put_resp = nav:put_file("localhost:3011/api/echo_put", put_path)
assert(put_resp ~= nil)
assert(put_resp.status == 200)
assert(put_resp.content == "lua put file")
