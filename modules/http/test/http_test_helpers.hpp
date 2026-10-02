#ifndef __HTTP_TEST_HELPERS_HPP__
#define __HTTP_TEST_HELPERS_HPP__

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/IWebsocketHandler.hpp>
#include <sihd/http/Multipart.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/WriteProtocol.hpp>
#include <sihd/http/request.hpp>
#include <sihd/json/Json.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

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
            ASSERT_TRUE(server.wait_ready(std::chrono::milliseconds(100)));
        }

        void stop()
        {
            server.set_service_wait_stop(true);
            server.stop();
        }

        ~ServerScope() { stop(); }
};

// WebService entry points shared by the scripting tests: echoes bodies, serves
// "hello" for GET/OPTIONS and reads back a multipart "field" on upload.
inline void setup_echo_webservice(sihd::http::WebService *webservice)
{
    webservice->set_entry_point("hello", [](const sihd::http::HttpRequest &, sihd::http::HttpResponse & resp) {
        resp.set_plain_content("navigator-ok");
    });
    webservice->set_entry_point(
        "hello",
        [](const sihd::http::HttpRequest &, sihd::http::HttpResponse & resp) { resp.set_plain_content("options-ok"); },
        sihd::http::HttpRequest::Options);
    webservice->set_entry_point(
        "echo",
        [](const sihd::http::HttpRequest & req, sihd::http::HttpResponse & resp) {
            resp.set_plain_content(req.content().cpp_str());
        },
        sihd::http::HttpRequest::Post);
    webservice->set_entry_point(
        "echo",
        [](const sihd::http::HttpRequest & req, sihd::http::HttpResponse & resp) {
            resp.set_plain_content(req.content().cpp_str());
        },
        sihd::http::HttpRequest::Patch);
    webservice->set_entry_point(
        "echo_put",
        [](const sihd::http::HttpRequest & req, sihd::http::HttpResponse & resp) {
            resp.set_plain_content(req.content().cpp_str());
        },
        sihd::http::HttpRequest::Put);
    webservice->set_entry_point(
        "upload",
        [](const sihd::http::HttpRequest & req, sihd::http::HttpResponse & resp) {
            const sihd::http::Multipart *multipart = req.multipart();
            if (multipart == nullptr)
            {
                resp.set_status(sihd::http::HttpStatus::BadRequest);
                return;
            }
            auto field = multipart->value("field");
            resp.set_plain_content(field.has_value() ? std::string(*field) : "no-field");
        },
        sihd::http::HttpRequest::Post);
}

struct EchoServerScope: ServerScope
{
        EchoServerScope() { setup_echo_webservice(server._webservice); }
};

// Test websocket + webservice server: counts every ws event, replies "hello world"
// to each client message and serves the /web entry points.
class SimpleHttpServer: public sihd::http::HttpServer,
                        public sihd::http::IWebsocketHandler
{
    public:
        SimpleHttpServer(): HttpServer("http-server-test")
        {
            // HttpServer protected call
            this->add_websocket("proto-two", this);
            _webservice = this->add_child<sihd::http::WebService>("web");
            this->setup_webservice_entry_points();
        }

        ~SimpleHttpServer() = default;

        void setup_webservice_entry_points()
        {
            _webservice->set_entry_point("some_get",
                                         [this](const sihd::http::HttpRequest &, sihd::http::HttpResponse & resp) {
                                             resp.set_plain_content("hello get world");
                                             ++_nget;
                                         });

            _webservice->set_entry_point(
                "some_post",
                [this](const sihd::http::HttpRequest & req, sihd::http::HttpResponse & resp) {
                    if (req.has_content())
                    {
                        _post_content = req.content().str();
                        resp.set_status(sihd::http::HttpStatus::Ok);
                        ++_npost;
                    }
                    else
                        resp.set_status(sihd::http::HttpStatus::BadRequest);
                },
                sihd::http::HttpRequest::Post);

            _webservice->set_entry_point(
                "some_delete",
                [this](const sihd::http::HttpRequest &, sihd::http::HttpResponse & resp) {
                    resp.set_status(sihd::http::HttpStatus::Ok);
                    resp.set_json_content({"hello", "world"});
                    ++_ndelete;
                },
                sihd::http::HttpRequest::Delete);

            _webservice->set_entry_point(
                "some_put",
                [this](const sihd::http::HttpRequest & req, sihd::http::HttpResponse & resp) {
                    if (req.has_content())
                    {
                        _put_content = req.content().str();
                        resp.set_status(sihd::http::HttpStatus::Ok);
                        ++_nput;
                    }
                    else
                        resp.set_status(sihd::http::HttpStatus::BadRequest);
                },
                sihd::http::HttpRequest::Put);
        }

        // IWebsocketHandler

        void on_open([[maybe_unused]] std::string_view protocol) override
        {
            {
                auto l = _ws_waitable.guard();
                ++_nopen;
            }
            _ws_waitable.notify_all();
        }

        bool on_read([[maybe_unused]] const sihd::util::ArrChar & arr) override
        {
            {
                auto l = _ws_waitable.guard();
                _client_wrote = true;
                ++_nread;
            }
            _ws_waitable.notify_all();
            return true;
        }

        bool on_write(sihd::util::ArrChar & arr, sihd::http::WriteProtocol & protocol) override
        {
            auto l = _ws_waitable.guard();
            if (_client_wrote)
            {
                ++_nwrite;
                _client_wrote = false;
                const char reply[] = "hello world";
                arr.from(reply);
                protocol = sihd::http::WriteProtocol::Text;
            }
            return true;
        }

        void on_close() override
        {
            {
                auto l = _ws_waitable.guard();
                ++_nclosed;
            }
            _ws_waitable.notify_all();
        }

        void on_peer_close([[maybe_unused]] uint16_t code, [[maybe_unused]] std::string_view reason) override {}

        bool wait_for_open(sihd::util::Duration timeout = sihd::util::Duration(sihd::util::time::sec(2)))
        {
            return _ws_waitable.wait_for(timeout, [this] { return _nopen > 0; });
        }

        bool wait_for_close(sihd::util::Duration timeout = sihd::util::Duration(sihd::util::time::sec(2)))
        {
            return _ws_waitable.wait_for(timeout, [this] { return _nclosed > 0; });
        }

        // websocket
        sihd::util::Waitable _ws_waitable;
        int _nopen = 0;
        int _nread = 0;
        int _nwrite = 0;
        int _nclosed = 0;
        bool _client_wrote = false;
        // webservice
        int _npost = 0;
        int _nput = 0;
        int _ndelete = 0;
        int _nget = 0;
        sihd::http::WebService *_webservice;
        std::string _post_content;
        std::string _put_content;
};

// Minimal HTTP CONNECT proxy with mandatory Basic proxy-auth. Test fixture only.
class SimpleConnectProxy
{
    public:
        SimpleConnectProxy(std::string user, std::string pass):
            _expected_token(sihd::util::str::to_base64(user + ":" + pass))
        {
        }

        ~SimpleConnectProxy() { stop(); }

        bool start(int port)
        {
            if (!_listen.open(AF_INET, SOCK_STREAM, IPPROTO_TCP))
                return false;
            (void)_listen.set_reuseaddr(true);
            if (!_listen.bind(sihd::net::IpAddr("127.0.0.1", port)) || !_listen.listen(16))
                return false;
            _port = port;
            _running = true;
            _accept_thread = std::thread([this] { _accept_loop(); });
            return true;
        }

        void stop()
        {
            if (!_running.exchange(false))
                return;
            // wake the blocking accept(): close() alone does not unblock it
            sihd::net::Socket waker;
            if (waker.open(AF_INET, SOCK_STREAM, IPPROTO_TCP))
                (void)waker.connect(sihd::net::IpAddr("127.0.0.1", _port));
            if (_accept_thread.joinable())
                _accept_thread.join();
            (void)waker.close();
            (void)_listen.close();
            std::lock_guard lock(_mutex);
            for (auto & t : _handlers)
                if (t.joinable())
                    t.join();
            _handlers.clear();
        }

        std::atomic<int> n_200 {0};
        std::atomic<int> n_407 {0};

    private:
        static std::string _lower(std::string s)
        {
            for (char & c : s)
                c = char(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        bool _auth_ok(const std::string & value) const
        {
            auto sp = value.find(' ');
            if (sp == std::string::npos)
                return false;
            return _lower(value.substr(0, sp)) == "basic" && value.substr(sp + 1) == _expected_token;
        }

        void _accept_loop()
        {
            while (_running)
            {
                sihd::net::IpAddr client_ip;
                auto fd = _listen.accept(client_ip);
                if (!fd)
                    break;
                if (!_running)
                {
                    sihd::net::Socket discard(fd.value());
                    break;
                }
                std::lock_guard lock(_mutex);
                _handlers.emplace_back([this, fd = fd.value()] { _handle(fd); });
            }
        }

        void _pipe(sihd::net::Socket & src, sihd::net::Socket & dst)
        {
            std::vector<char> tmp(65536);
            while (_running && src.is_open() && dst.is_open())
            {
                auto n = src.receive(tmp.data(), tmp.size());
                if (!n || n.value() == 0)
                    break;
                if (!dst.send_all(sihd::util::ArrCharView(tmp.data(), n.value())))
                    break;
            }
            (void)src.shutdown();
            (void)dst.shutdown();
        }

        void _handle(int fd)
        {
            sihd::net::Socket client(fd);
            std::string buf;
            std::vector<char> tmp(4096);
            while (buf.find("\r\n\r\n") == std::string::npos)
            {
                auto n = client.receive(tmp.data(), tmp.size());
                if (!n || n.value() == 0)
                    return;
                buf.append(tmp.data(), n.value());
            }

            auto head_end = buf.find("\r\n\r\n");
            std::string rest = buf.substr(head_end + 4);
            std::istringstream iss(buf.substr(0, head_end));

            std::string line;
            std::getline(iss, line);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            std::string method, target;
            std::istringstream(line) >> method >> target;

            std::string proxy_auth;
            while (std::getline(iss, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                auto col = line.find(':');
                if (col == std::string::npos)
                    continue;
                std::string key = _lower(line.substr(0, col));
                std::string val = line.substr(col + 1);
                while (!val.empty() && val.front() == ' ')
                    val.erase(0, 1);
                if (key == "proxy-authorization")
                    proxy_auth = val;
            }

            if (method != "CONNECT")
            {
                (void)client.send_all(std::string_view("HTTP/1.1 405 Method Not Allowed\r\n\r\n"));
                return;
            }
            if (!_auth_ok(proxy_auth))
            {
                ++n_407;
                (void)client.send_all(std::string_view(
                    "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"test\"\r\n\r\n"));
                return;
            }

            auto colon = target.rfind(':');
            std::string host = target.substr(0, colon);
            int port = std::stoi(target.substr(colon + 1));
            sihd::net::IpAddr resolved = sihd::net::dns::find(host).value();
            sihd::net::IpAddr up_addr(resolved.empty() ? host : resolved.str(), port);
            sihd::net::Socket upstream;
            if (!upstream.open(AF_INET, SOCK_STREAM, IPPROTO_TCP) || !upstream.connect(up_addr))
            {
                (void)client.send_all(std::string_view("HTTP/1.1 502 Bad Gateway\r\n\r\n"));
                return;
            }
            ++n_200;
            (void)client.send_all(std::string_view("HTTP/1.1 200 Connection Established\r\n\r\n"));
            if (!rest.empty())
                (void)upstream.send_all(sihd::util::ArrCharView(rest.data(), rest.size()));

            std::thread up([&] { _pipe(client, upstream); });
            _pipe(upstream, client);
            up.join();
        }

        sihd::net::Socket _listen;
        std::string _expected_token;
        int _port = 0;
        std::atomic<bool> _running {false};
        std::thread _accept_thread;
        std::vector<std::thread> _handlers;
        std::mutex _mutex;
};

} // namespace test

#endif
