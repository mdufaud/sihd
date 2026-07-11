import sihd

nav = sihd.http.Navigator()
nav.clear_proxy()
nav.set_timeout(5)

# GET
resp = nav.get("localhost:3012/api/hello")
assert(resp is not None)
assert(resp.status() == 200)
assert(resp.text() == "navigator-ok")
assert(resp.content() == b"navigator-ok")
assert(isinstance(resp.headers(), dict))
assert(isinstance(resp.cookies(), dict))
assert(isinstance(resp.redirect_history(), list))

# POST echo
echoed = nav.post("localhost:3012/api/echo", "hello py")
assert(echoed is not None)
assert(echoed.status() == 200)
assert(echoed.text() == "hello py")

# unknown route yields a response (404), not None
missing = nav.get("localhost:3012/api/nope")
assert(missing is not None)
assert(missing.status() == 404)

# CurlOptions is a named-field struct
opt = sihd.http.CurlOptions()
opt.timeout_s = 5
opt.headers = {"X-Test": "1"}
opt.follow_location = True
assert(opt.timeout_s == 5)
assert(opt.headers["X-Test"] == "1")
assert(opt.proxy is None)

# stateless request helper consuming CurlOptions
r = sihd.http.get("localhost:3012/api/hello", opt)
assert(r is not None)
assert(r.status() == 200)
assert(r.text() == "navigator-ok")
