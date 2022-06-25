#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Str.hpp>
#include <sihd/util/FS.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Handler.hpp>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/WebsocketHandler.hpp>
#include <sihd/http/WebService.hpp>

namespace test
{
    SIHD_NEW_LOGGER("test");
    using namespace sihd::http;
    using namespace sihd::util;
    class TestHttpServer: public ::testing::Test
    {
        protected:
            TestHttpServer()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestHttpServer()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _cwd;
            std::string _base_test_dir;
    };

    class SimpleHttpServer: public sihd::http::HttpServer, public sihd::http::IWebsocketHandler
    {
        public:
            SimpleHttpServer(): HttpServer("http-server-test")
            {
                // HttpServer protected call
                this->_add_websocket("proto-two", this);
                _webservice = this->add_child<WebService>("web");
                this->setup_webservice_entry_points();
            }

            ~SimpleHttpServer()
            {
            }

            void setup_webservice_entry_points()
            {
                _webservice->set_entry_point("some_get", [this] (const HttpRequest & req, HttpResponse & resp)
                {
                    SIHD_LOG(info, req.request_to_string(req.request_type()) << " request received");
                    resp.set_content("hello get world");
                    ++_nget;
                });

                _webservice->set_entry_point("some_post", [this] (const HttpRequest & req, HttpResponse & resp)
                {
                    SIHD_LOG(info, req.request_to_string(req.request_type()) << " request received");
                    if (req.content())
                    {
                        _post_content = req.content()->cpp_str();
                        SIHD_LOG(info, "Received POST body: " << _post_content);
                        resp.http_header().set_status(HTTP_STATUS_OK);
                        ++_npost;
                    }
                    else
                        resp.http_header().set_status(HTTP_STATUS_BAD_REQUEST);
                },
                HttpRequest::POST);

                _webservice->set_entry_point("some_delete", [this] (const HttpRequest & req, HttpResponse & resp)
                {
                    SIHD_LOG(info, req.request_to_string(req.request_type()) << " request received");
                    resp.http_header().set_status(HTTP_STATUS_OK);
                    resp.set_json_content({"hello", "world"});
                    ++_ndelete;
                },
                HttpRequest::REQ_DELETE);

                _webservice->set_entry_point("some_put", [this] (const HttpRequest & req, HttpResponse & resp)
                {
                    SIHD_LOG(info, req.request_to_string(req.request_type()) << " request received");
                    if (req.content())
                    {
                        _post_content = req.content()->cpp_str();
                        SIHD_LOG(info, "Received PUT body: " << _post_content);
                        resp.http_header().set_status(HTTP_STATUS_OK);
                        ++_nput;
                    }
                    else
                        resp.http_header().set_status(HTTP_STATUS_BAD_REQUEST);
                },
                HttpRequest::PUT);
            }

            // IWebsocketHandler

            void on_open(const char *protocol_name)
            {
                SIHD_LOG(debug, "Opened websocket of protocol: " << protocol_name);
                ++_nopen;
            };

            bool on_read(const sihd::util::ArrChar & array)
            {
                SIHD_LOG(debug, "Read from client websocket: " << array.str());
                _client_wrote = true;
                ++_nread;
                return true;
            };

            bool on_write(sihd::util::ArrChar & array, LwsWriteProtocol *protocol)
            {
                if (_client_wrote)
                {
                    _client_wrote = false;
                    const char hw[] = "hello world";
                    protocol->write_protocol = LWS_WRITE_TEXT;
                    array.from(hw);
                    SIHD_LOG(debug, "Wrote back to client websocket: " << hw);
                    ++_nwrite;
                }
                return true;
            }

            void on_close()
            {
                SIHD_LOG(debug, "Closed websocket");
                ++_nclosed;
            }

            // websocket
            int _nopen = 0;
            int _nread = 0;
            int _nwrite = 0;
            int _nclosed = 0;
            bool _client_wrote = false;
            WebsocketHandler _websocket_handler;
            // webservice
            int _npost = 0;
            int _nput = 0;
            int _ndelete = 0;
            int _nget = 0;
            WebService *_webservice;
            std::string _post_content;
    };

    TEST_F(TestHttpServer, test_httpserver)
    {
        if (sihd::util::Term::is_interactive() == false)
            GTEST_SKIP_("requires interaction");

        SimpleHttpServer server;
        OS::add_signal_handler(SIGINT, new Handler<int>([&server] (int sig)
        {
            (void)sig;
            server.stop();
        }));
        server.set_root_dir("test/resources/mount_point");
        server.set_port(3000);
        SIHD_LOG(info, "=========================================================");
        SIHD_LOG(info, "Open web browser at localhost:3000");
        SIHD_LOG(info, "=========================================================");
        server.run();
        EXPECT_GT(server._nopen, 0);
        EXPECT_GT(server._nread, 0);
        EXPECT_GT(server._nwrite, 0);
        EXPECT_GT(server._nclosed, 0);
        //
        EXPECT_GT(server._nget, 0);
        EXPECT_GT(server._npost, 0);
        EXPECT_GT(server._nput, 0);
        EXPECT_GT(server._ndelete, 0);
    }
}