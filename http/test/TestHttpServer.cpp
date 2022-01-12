#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Str.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/util/Runnable.hpp>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/WebsocketHandler.hpp>


namespace test
{
    NEW_LOGGER("test");
    using namespace sihd::http;
    using namespace sihd::util;
    class TestHttpServer: public ::testing::Test
    {
        protected:
            TestHttpServer()
            {
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::Files::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "http",
                    "httpserver"
                });
                _cwd = sihd::util::OS::get_cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
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
                this->_add_websocket(_name, this);
            }

            ~SimpleHttpServer()
            {
            }

            // IWebsocketHandler

            void on_open(const char *protocol_name)
            {
                TRACE(protocol_name);
                ++_nopen;
            };

            bool on_read(const sihd::util::ArrStr & array)
            {
                TRACE(array.to_string());
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
                }
                return true;
            }

            void on_close()
            {
                ++_nclosed;
            }

            const char *_name = "proto-two";
            int _nopen = 0;
            int _nclosed = 0;
            bool _client_wrote = false;
            WebsocketHandler _websocket_handler;
    };

    TEST_F(TestHttpServer, test_httpserver)
    {
        if (sihd::util::Term::is_interactive() == false)
            GTEST_SKIP_("requires interaction");

        SimpleHttpServer server;
        OS::add_signal_handler(SIGINT, new Runnable([&server] () -> bool
        {
            server.stop();
            return true;
        }));
        server.set_root_dir("test/resources/mount_point");
        server.set_port(3000);
        server.run();
    }
}