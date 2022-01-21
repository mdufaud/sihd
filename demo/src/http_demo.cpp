#include <sihd/util/platform.hpp>

#include <sihd/util/Node.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Str.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Runnable.hpp>
#include <sihd/util/Handler.hpp>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/WebsocketHandler.hpp>
#include <sihd/http/WebService.hpp>

#include <unistd.h> // usleep

namespace test::module
{

using namespace sihd::util;
using namespace sihd::http;

NEW_LOGGER("test::module");

class SimpleHttpServer: public sihd::http::HttpServer, public sihd::http::IWebsocketHandler
{
    public:
        SimpleHttpServer(): HttpServer("simple-http-server")
        {
            this->_add_websocket("websocket-protocol", this);
            _webservice = this->add_child<WebService>("webservice");
            this->setup_webservice_entry_points();
        }

        ~SimpleHttpServer()
        {
        }

        void setup_webservice_entry_points()
        {
            _webservice->set_entry_point("get", [this] (const HttpRequest & req, HttpResponse & resp)
            {
                LOG(info, req.request_to_string(req.request_type()) << " request received");
                resp.set_content("hello get world");
            });

            _webservice->set_entry_point("post", [this] (const HttpRequest & req, HttpResponse & resp)
            {
                LOG(info, req.request_to_string(req.request_type()) << " request received");
                if (req.content())
                {
                    std::string content = req.content()->cpp_str();
                    LOG(info, "Received POST body: " << content);
                    resp.http_header().set_status(HTTP_STATUS_OK);
                }
                else
                    resp.http_header().set_status(HTTP_STATUS_BAD_REQUEST);
            },
            HttpRequest::POST);

            _webservice->set_entry_point("delete", [this] (const HttpRequest & req, HttpResponse & resp)
            {
                LOG(info, req.request_to_string(req.request_type()) << " request received");
                resp.http_header().set_status(HTTP_STATUS_OK);
                resp.set_json_content({"hello", "world"});
            },
            HttpRequest::REQ_DELETE);

            _webservice->set_entry_point("put", [this] (const HttpRequest & req, HttpResponse & resp)
            {
                LOG(info, req.request_to_string(req.request_type()) << " request received");
                if (req.content())
                {
                    std::string content = req.content()->cpp_str();
                    LOG(info, "Received PUT body: " << content);
                    resp.http_header().set_status(HTTP_STATUS_OK);
                }
                else
                    resp.http_header().set_status(HTTP_STATUS_BAD_REQUEST);
            },
            HttpRequest::PUT);
        }

        // IWebsocketHandler

        void on_open(const char *protocol_name)
        {
            LOG(debug, "Opened websocket of protocol: " << protocol_name);
        };

        bool on_read(const sihd::util::ArrStr & array)
        {
            LOG(debug, "Read from client websocket: " << array.to_string());
            _client_wrote = true;
            return true;
        };

        bool on_write(sihd::util::ArrStr & array, LwsWriteProtocol *protocol)
        {
            if (_client_wrote)
            {
                _client_wrote = false;
                const char hw[] = "hello world";
                protocol->write_protocol = LWS_WRITE_TEXT;
                array.from(hw);
                LOG(debug, "Wrote back to client websocket: " << hw);
            }
            return true;
        }

        void on_close()
        {
            LOG(debug, "Closed websocket");
        }

        // websocket
        WebsocketHandler _websocket_handler;
        bool _client_wrote = false;
        // webservice
        WebService *_webservice;
};

static void http_test()
{
        SimpleHttpServer server;
        OS::add_signal_handler(SIGINT, new Runnable([&server] () -> bool
        {
            server.stop();
            return true;
        }));
        std::string root_path = Files::get_parent(Files::get_parent(OS::get_executable_path()));
        std::string res_path = Files::combine({root_path, "etc", "sihd", "demo", "http_demo"});
        LOG(info, "Root dir: " << res_path);
        server.set_root_dir(res_path);
        server.set_port(3000);
        LOG(info, "=========================================================");
        LOG(info, "Open web browser at localhost:3000");
        LOG(info, "=========================================================");
        server.run();
}

} // end test::module

int main()
{
    sihd::util::Str::hexdump_cols = 20;
    sihd::util::LoggerManager::basic();
    test::module::http_test();
    return 0;
}


