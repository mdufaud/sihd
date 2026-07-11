import sihd
import threading

server = sihd.http.HttpServer("py-server")
assert server.set_port(3013)

svc = server.add_web_service("api")

def compute(req, resp):
    resp.set_status(200)
    resp.set_plain_content("from-python:" + req.text())

svc.set_entry_point("compute", compute, sihd.http.RequestType.Post)

t = threading.Thread(target=server.start)
t.start()
try:
    assert server.wait_ready(2000)

    nav = sihd.http.Navigator()
    nav.clear_proxy()
    nav.set_timeout(5)

    resp = nav.post("localhost:3013/api/compute", "payload")
    assert resp is not None
    assert resp.status() == 200
    assert resp.text() == "from-python:payload"
finally:
    server.set_service_wait_stop(True)
    server.request_stop()
    server.stop()
    t.join()
