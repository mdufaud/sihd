local http = sihd.http

server = http.HttpServer("lua-server", nil)
assert(server:set_port(3021))

local svc = server:add_web_service("api")
svc:set_entry_point("compute", function(req, resp)
    resp:set_status(200)
    resp:set_plain_content("from-lua:" .. req:text())
end, "POST")

svc:set_entry_point("upload", function(req, resp)
    if req:has_multipart() == false then
        resp:set_status(400)
        return
    end
    for _, part in ipairs(req:multipart()) do
        if part.name == "field" then
            resp:set_status(200)
            resp:set_plain_content("lua-multipart:" .. part.data)
            return
        end
    end
    resp:set_status(400)
end, "POST")
