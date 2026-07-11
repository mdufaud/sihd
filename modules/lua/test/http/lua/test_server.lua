local http = sihd.http

server = http.HttpServer("lua-server", nil)
assert(server:set_port(3021))

local svc = server:add_web_service("api")
svc:set_entry_point("compute", function(req, resp)
    resp:set_status(200)
    resp:set_plain_content("from-lua:" .. req:text())
end, "POST")
