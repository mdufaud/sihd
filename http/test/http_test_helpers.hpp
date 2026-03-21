#ifndef __HTTP_TEST_HELPERS_HPP__
#define __HTTP_TEST_HELPERS_HPP__

#include <chrono>
#include <string_view>

#include <gtest/gtest.h>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/IWebsocketHandler.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/WriteProtocol.hpp>
#include <sihd/util/Worker.hpp>

namespace test
{

class TestFilter: public sihd::http::IHttpFilter
{
    public:
        std::string blocked_uri;
        std::string last_uri;
        std::string last_client_ip;
        int filter_calls = 0;

        bool on_filter_connection(const sihd::http::HttpFilterInfo & info) override
        {
            last_uri = std::string(info.uri);
            last_client_ip = info.client_ip;
            ++filter_calls;
            if (!blocked_uri.empty() && info.uri.find(blocked_uri) != std::string_view::npos)
                return false;
            return true;
        }
};

class TestAuth: public sihd::http::IHttpAuthenticator
{
    public:
        std::string valid_user = "admin";
        std::string valid_pass = "secret";
        std::string valid_token = "my-secret-token-123";

        bool on_basic_auth(std::string_view username, std::string_view password) override
        {
            return username == valid_user && password == valid_pass;
        }

        bool on_token_auth(std::string_view token) override { return token == valid_token; }
};

class FeatureHttpServer: public sihd::http::HttpServer
{
    public:
        FeatureHttpServer(): HttpServer("feature-server")
        {
            _webservice = this->add_child<sihd::http::WebService>("api");
        }

        ~FeatureHttpServer() = default;

        sihd::http::WebService *_webservice;
};

struct ServerScope
{
        FeatureHttpServer server;
        sihd::util::Worker worker;

        ServerScope():
            worker([this] {
                server.start();
                return true;
            })
        {
        }

        void start(int port = 3001)
        {
            server.set_port(port);
            ASSERT_TRUE(worker.start_sync_worker("test-server"));
            server.wait_ready(std::chrono::milliseconds(100));
        }

        void stop()
        {
            server.set_service_wait_stop(true);
            server.stop();
        }

        ~ServerScope() { stop(); }
};

class SimpleWsServer: public sihd::http::HttpServer,
                      public sihd::http::IWebsocketHandler
{
    public:
        SimpleWsServer(): HttpServer("ws-server-test") { this->add_websocket("proto-two", this); }

        ~SimpleWsServer() = default;

        void on_open([[maybe_unused]] std::string_view protocol) override { ++_nopen; }
        void on_close() override { ++_nclosed; }
        void on_peer_close([[maybe_unused]] uint16_t code, [[maybe_unused]] std::string_view reason) override
        {
        }

        bool on_read([[maybe_unused]] const sihd::util::ArrChar & arr) override
        {
            ++_nread;
            _should_reply = true;
            return true;
        }

        bool on_write(sihd::util::ArrChar & arr, sihd::http::WriteProtocol & protocol) override
        {
            if (_should_reply)
            {
                _should_reply = false;
                const char reply[] = "hello world";
                arr.from(reply);
                protocol = sihd::http::WriteProtocol::Text;
                ++_nwrite;
            }
            return true;
        }

        int _nopen = 0;
        int _nread = 0;
        int _nwrite = 0;
        int _nclosed = 0;
        bool _should_reply = false;
};

} // namespace test

#endif
