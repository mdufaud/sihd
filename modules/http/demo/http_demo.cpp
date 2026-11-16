#include <thread>

#include <sihd/http/AWebsocketHandler.hpp>
#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/json/Json.hpp>
#include <sihd/sys/App.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/sys/program.hpp>
#include <sihd/util/CliApp.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/ansi.hpp>
#include <sihd/util/str.hpp>

namespace demo
{

using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::http;

SIHD_NEW_LOGGER("demo");

class HelloWebsocket: public AWebsocketHandler
{
    public:
        using AWebsocketHandler::AWebsocketHandler;

        void on_open() override { SIHD_LOG(debug, "Opened websocket of protocol: {}", this->protocol_name()); }

        void on_text(std::string_view text) override
        {
            SIHD_LOG(debug, "Read from client websocket: {}", text);
            if (this->send("hello world"))
                SIHD_LOG(debug, "Wrote back to client websocket: hello world");
        }

        void on_peer_close(uint16_t code, std::string_view reason) override
        {
            SIHD_LOG(debug, "Peer closed websocket: code={} reason={}", code, reason);
        }

        void on_close() override { SIHD_LOG(debug, "Closed websocket"); }
};

class SimpleHttpServer: public sihd::http::HttpServer
{
    public:
        SimpleHttpServer(): HttpServer("simple-http-server")
        {
            this->add_websocket<HelloWebsocket>("websocket-protocol");
            _webservice = this->add_child<WebService>("webservice");
            this->setup_webservice_entry_points();
        }

        ~SimpleHttpServer() = default;

        void setup_webservice_entry_points()
        {
            _webservice->set_entry_point("get", [](const HttpRequest & req, HttpResponse & resp) {
                SIHD_LOG(info, "{} request received", req.type_str());
                resp.set_plain_content("hello get world");
            });

            _webservice->set_entry_point(
                "post",
                [](const HttpRequest & req, HttpResponse & resp) {
                    SIHD_LOG(info, "{} request received", req.type_str());
                    if (req.has_content())
                    {
                        std::string content = req.content().str();
                        SIHD_LOG(info, "Received POST body: {}", content);
                        resp.set_status(HttpStatus::Ok);
                    }
                    else
                        resp.set_status(HttpStatus::BadRequest);
                },
                HttpRequest::Post);

            _webservice->set_entry_point(
                "delete",
                [](const HttpRequest & req, HttpResponse & resp) {
                    SIHD_LOG(info, "{} request received", req.type_str());
                    resp.set_status(HttpStatus::Ok);
                    resp.set_json_content({"hello", "world"});
                },
                HttpRequest::Delete);

            _webservice->set_entry_point(
                "put",
                [](const HttpRequest & req, HttpResponse & resp) {
                    SIHD_LOG(info, "{} request received", req.type_str());
                    if (req.has_content())
                    {
                        std::string content = req.content().str();
                        SIHD_LOG(info, "Received PUT body: {}", content);
                        resp.set_status(HttpStatus::Ok);
                    }
                    else
                        resp.set_status(HttpStatus::BadRequest);
                },
                HttpRequest::Put);
        }

        WebService *_webservice;
};

namespace
{

void http_test(CliApp & app)
{
    SimpleHttpServer server;

    std::string root_path = fs::parent(fs::parent(sihd::sys::program::path()));
    std::string res_path = fs::combine({root_path, "etc", "sihd", "demo", "http_demo"});
    SIHD_LOG(info, "Root dir: {}", res_path);
    server.set_root_dir(res_path);
    server.set_port(3000);
    SIHD_LOG(info, "=========================================================");
    SIHD_LOG(info, "Open web browser at http://localhost:3000");
    SIHD_LOG(info, "=========================================================");

    // the server blocks in start(); its end or a stop signal breaks the loop
    std::thread server_thread([&] {
        server.start();
        app.stop();
    });
    app.loop();
    server.stop();
    server_thread.join();
}

} // namespace

} // namespace demo

int main(int argc, char **argv)
{
    sihd::sys::App app({
        .name = "http_demo",
        .description = "Simple http server demo",
    });

    app.root().on_run([&app] { demo::http_test(app); });

    return app.run(argc, argv);
}
