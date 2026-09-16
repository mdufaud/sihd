import sihd

nav = sihd.http.Navigator()
nav.clear_proxy()
nav.set_timeout(sihd.util.time.sec(5))

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

# RequestOptions is a named-field struct
opt = sihd.http.RequestOptions()
opt.timeout = sihd.util.time.sec(5)
opt.headers = {"X-Test": "1"}
opt.follow_location = True
assert(int(opt.timeout) == sihd.util.time.sec(5))
assert(opt.headers["X-Test"] == "1")
assert(opt.proxy is None)
assert(opt.ssl_verify_peer)
assert(opt.ssl_verify_host)

# durations are nanoseconds, sihd.util.time builds them
seconds = sihd.http.RequestOptions()
seconds.timeout = sihd.util.time.sec(5)
assert(int(seconds.timeout) == 5000000000)
assert(int(seconds.timeout) == sihd.util.time.sec(5))
seconds.connect_timeout = sihd.util.time.ms(2500)
assert(int(seconds.connect_timeout) == sihd.util.time.ms(2500))

# the method comes from the helper called, not from the options
assert(not hasattr(seconds, "type"))

# stateless request helper consuming RequestOptions
r = sihd.http.get("localhost:3012/api/hello", opt)
assert(r is not None)
assert(r.status() == 200)
assert(r.text() == "navigator-ok")

patched = sihd.http.patch("localhost:3012/api/echo", "hello patch", opt)
assert(patched is not None)
assert(patched.status() == 200)
assert(patched.text() == "hello patch")

# CORS is on by default: a preflight carries origin and access-control-request-method
preflight = sihd.http.RequestOptions()
preflight.headers = {"Origin": "https://app.com", "Access-Control-Request-Method": "POST"}
pre = sihd.http.options("localhost:3012/api/echo", preflight)
assert(pre is not None)
assert(pre.status() == 204)
assert(pre.headers()["access-control-allow-methods"] == "GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS")

# a plain OPTIONS reaches the routes: known path, method without a route
plain = sihd.http.options("localhost:3012/api/echo", opt)
assert(plain is not None)
assert(plain.status() == 405)
assert(plain.headers()["allow"] == "POST, PATCH")

# an OPTIONS route is answered with its body
handled = sihd.http.options("localhost:3012/api/hello", opt)
assert(handled is not None)
assert(handled.status() == 200)
assert(handled.text() == "options-ok")

# unknown path still yields 404
nowhere = sihd.http.options("localhost:3012/api/nope", opt)
assert(nowhere is not None)
assert(nowhere.status() == 404)

# multipart: the navigator sends it, the options can carry one too
form = sihd.http.Multipart()
form.add_field("field", "multipart-value")
assert(not form.empty())
assert(form.value("field") == "multipart-value")

uploaded = nav.post_multipart("localhost:3012/api/upload", form)
assert(uploaded is not None)
assert(uploaded.status() == 200)
assert(uploaded.text() == "multipart-value")

with_form = sihd.http.RequestOptions()
with_form.multipart = form
sent = sihd.http.post("localhost:3012/api/upload", b"", with_form)
assert(sent is not None)
assert(sent.status() == 200)
assert(sent.text() == "multipart-value")

# Navigator: sequential requests on one connection pool
reuse = sihd.http.Navigator()
reuse.clear_proxy()
reuse.set_timeout(sihd.util.time.sec(5))
r1 = reuse.get("localhost:3012/api/hello")
assert(r1 is not None)
assert(r1.status() == 200)
assert(r1.text() == "navigator-ok")
assert(reuse.new_connection_count() >= 1)

r2 = reuse.get("localhost:3012/api/hello")
assert(r2 is not None)
assert(r2.text() == "navigator-ok")
assert(reuse.new_connection_count() == 0)

r3 = reuse.post("localhost:3012/api/echo", "hello reuse")
assert(r3 is not None)
assert(r3.text() == "hello reuse")
assert(reuse.new_connection_count() == 0)

# Navigator.last_error: the reason of the last failed request, cleared by the next one
assert(reuse.last_error() == "")
failed = reuse.get("localhost:19999/api/hello")
assert(failed is None)
assert(len(reuse.last_error()) > 0)

recovered = reuse.get("localhost:3012/api/hello")
assert(recovered is not None)
assert(recovered.status() == 200)
assert(reuse.last_error() == "")
