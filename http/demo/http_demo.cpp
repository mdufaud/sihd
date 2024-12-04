#include <unistd.h> // usleep

#include <csignal>
#include <nlohmann/json.hpp>

#include <libwebsockets.h>

#include <sihd/util/File.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Process.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/SigWatcher.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/WebsocketHandler.hpp>

namespace demo
{

using namespace sihd::util;
using namespace sihd::http;

SIHD_NEW_LOGGER("demo");

class SimpleHttpServer: public sihd::http::HttpServer,
                        public sihd::http::IWebsocketHandler
{
    public:
        SimpleHttpServer(): HttpServer("simple-http-server")
        {
            this->add_websocket("websocket-protocol", this);
            _webservice = this->add_child<WebService>("webservice");
            this->setup_webservice_entry_points();
        }

        ~SimpleHttpServer() {}

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
                        resp.set_status(HTTP_STATUS_OK);
                    }
                    else
                        resp.set_status(HTTP_STATUS_BAD_REQUEST);
                },
                HttpRequest::Post);

            _webservice->set_entry_point(
                "delete",
                [](const HttpRequest & req, HttpResponse & resp) {
                    SIHD_LOG(info, "{} request received", req.type_str());
                    resp.set_status(HTTP_STATUS_OK);
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
                        resp.set_status(HTTP_STATUS_OK);
                    }
                    else
                        resp.set_status(HTTP_STATUS_BAD_REQUEST);
                },
                HttpRequest::Put);
        }

        // IWebsocketHandler

        void on_open(std::string_view protocol_name)
        {
            SIHD_LOG(debug, "Opened websocket of protocol: {}", protocol_name);
        };

        bool on_read(const sihd::util::ArrChar & array)
        {
            SIHD_LOG(debug, "Read from client websocket: {}", array.str());
            _client_wrote = true;
            return true;
        };

        bool on_write(sihd::util::ArrChar & array, LwsWriteProtocol & protocol)
        {
            if (_client_wrote)
            {
                _client_wrote = false;

                std::string hw("hello world");
                array.from(hw);

                protocol.set_txt();
                SIHD_LOG(debug, "Wrote back to client websocket: {}", hw);
            }
            return true;
        }

        void on_close() { SIHD_LOG(debug, "Closed websocket"); }

        bool _client_wrote = false;
        // websocket
        WebsocketHandler _websocket_handler;
        // webservice
        WebService *_webservice;
};

static void http_test()
{
    SimpleHttpServer server;

    sihd::util::SigWatcher watcher("signal-watcher");

    watcher.add_signal(SIGINT);
    watcher.set_polling_frequency(5);

    Handler<SigWatcher *> sig_handler([&server](SigWatcher *watcher) {
        auto & catched_signals = watcher->catched_signals();
        if (catched_signals.empty())
            return;
        SIHD_LOG(info, "Stopping http server...");
        server.stop();
        SIHD_LOG(info, "Stopped http server");
    });
    watcher.add_observer(&sig_handler);
    watcher.start();

    std::string root_path = fs::parent(fs::parent(fs::executable_path()));
    std::string res_path = fs::combine({root_path, "etc", "sihd", "demo", "http_demo"});
    SIHD_LOG(info, "Root dir: {}", res_path);
    server.set_root_dir(res_path);
    server.set_port(3000);
    SIHD_LOG(info, "=========================================================");
    SIHD_LOG(info, "Open web browser at http://localhost:3000");
    SIHD_LOG(info, "=========================================================");
    server.start();
}

} // namespace demo

int main()
{
    sihd::util::LoggerManager::stream();
    demo::http_test();
    sihd::util::LoggerManager::clear_loggers();
    return 0;
}
