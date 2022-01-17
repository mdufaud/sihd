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

#include <sihd/pcap/Sniffer.hpp>
#include <sihd/pcap/PcapInterfaces.hpp>

#if defined(__SIHD_WINDOWS__)
// prevents error: previous declaration as 'typedef long int suseconds_t'
// windows libwebsockets - contrary to libpcap
// have a way to not typedef based on this define
# define LWS_HAVE_SUSECONDS_T
#endif

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/WebsocketHandler.hpp>
#include <sihd/http/WebService.hpp>

#include <unistd.h> // usleep

namespace test::module
{

using namespace sihd::util;
using namespace sihd::pcap;
using namespace sihd::http;

NEW_LOGGER("test::module");

static void node_test()
{
    Node node("root");
    LOG(info, "Node root name: " << node.get_name());
    LOG(info, "Executable path: " << OS::get_executable_path());
    std::cout << std::endl;
}

static std::string interfaces_test()
{
    LOG(info, "Net interfaces");
    std::string interface_to_sniff;
    PcapInterfaces ifaces;
    for (const auto & iface: ifaces.ifaces())
    {
        LOG(info, iface.dump());
        if (iface.up() && !iface.loopback())
        {
            interface_to_sniff = iface.name();
        }
    }
    std::cout << std::endl;
    return interface_to_sniff;
}

static void sniffer_test(const std::string & interface_to_sniff)
{
    LOG(info, "Sniffing on eth0");
    Sniffer pcap("pcap-sniffer");
    Handler<Sniffer *> obs([] (Sniffer *obj)
    {
        // TRACE(obj->array().hexdump());
        LOG(info, obj->array().size());
        std::cout << Str::hexdump_fmt(obj->array().cbuf(), obj->array().byte_size()) << std::endl;
    });
    pcap.add_observer(&obs);
    pcap.open(interface_to_sniff);
    if (pcap.is_open() == true)
    {
        // pcap.set_monitor(true);
        // pcap.set_immediate(true);
        pcap.set_promiscuous(false);
        pcap.set_snaplen(2048);
        pcap.set_timeout(512);
        LOG(info, "Activating packet capture");
        pcap.activate();
        if (pcap.is_active())
        {
            pcap.set_filter("portrange 0-2000");
            // sniff once
            pcap.sniff();
            usleep(10E3);
            pcap.close();
        }
    }
    LOG(info, "Ending packet capture");
    std::cout << std::endl;
}

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
        std::string res_path = Files::combine({root_path, "etc", "sihd", "_module_test"});
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
    test::module::node_test();
    std::string interface_to_sniff = test::module::interfaces_test();
    test::module::sniffer_test(interface_to_sniff);
    test::module::http_test();
    return 0;
}