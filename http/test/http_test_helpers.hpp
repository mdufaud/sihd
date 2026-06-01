#ifndef __HTTP_TEST_HELPERS_HPP__
#define __HTTP_TEST_HELPERS_HPP__

#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/IWebsocketHandler.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/WriteProtocol.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/util/ArrayView.hpp>
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
            ASSERT_TRUE(server.wait_ready(std::chrono::milliseconds(100)));
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

// Minimal HTTP CONNECT proxy with mandatory Basic proxy-auth. Test fixture only.
class SimpleConnectProxy
{
    public:
        SimpleConnectProxy(std::string user, std::string pass):
            _expected_token(_base64(user + ":" + pass))
        {
        }

        ~SimpleConnectProxy() { stop(); }

        bool start(int port)
        {
            if (!_listen.open(AF_INET, SOCK_STREAM, IPPROTO_TCP))
                return false;
            _listen.set_reuseaddr(true);
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
                waker.connect(sihd::net::IpAddr("127.0.0.1", _port));
            if (_accept_thread.joinable())
                _accept_thread.join();
            waker.close();
            _listen.close();
            std::lock_guard lock(_mutex);
            for (auto & t : _handlers)
                if (t.joinable())
                    t.join();
            _handlers.clear();
        }

        std::atomic<int> n_200 {0};
        std::atomic<int> n_407 {0};

    private:
        static std::string _base64(std::string_view in)
        {
            static constexpr char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            size_t i = 0;
            for (; i + 2 < in.size(); i += 3)
            {
                uint32_t n = (uint32_t(uint8_t(in[i])) << 16) | (uint32_t(uint8_t(in[i + 1])) << 8)
                             | uint32_t(uint8_t(in[i + 2]));
                out += tbl[(n >> 18) & 63];
                out += tbl[(n >> 12) & 63];
                out += tbl[(n >> 6) & 63];
                out += tbl[n & 63];
            }
            size_t rem = in.size() - i;
            if (rem == 1)
            {
                uint32_t n = uint32_t(uint8_t(in[i])) << 16;
                out += tbl[(n >> 18) & 63];
                out += tbl[(n >> 12) & 63];
                out += "==";
            }
            else if (rem == 2)
            {
                uint32_t n = (uint32_t(uint8_t(in[i])) << 16) | (uint32_t(uint8_t(in[i + 1])) << 8);
                out += tbl[(n >> 18) & 63];
                out += tbl[(n >> 12) & 63];
                out += tbl[(n >> 6) & 63];
                out += '=';
            }
            return out;
        }

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
                int fd = _listen.accept(client_ip);
                if (fd < 0)
                    break;
                if (!_running)
                {
                    sihd::net::Socket discard(fd);
                    break;
                }
                std::lock_guard lock(_mutex);
                _handlers.emplace_back([this, fd] { _handle(fd); });
            }
        }

        void _pipe(sihd::net::Socket & src, sihd::net::Socket & dst)
        {
            std::vector<char> tmp(65536);
            while (_running && src.is_open() && dst.is_open())
            {
                ssize_t n = src.receive(tmp.data(), tmp.size());
                if (n <= 0)
                    break;
                if (!dst.send_all(sihd::util::ArrCharView(tmp.data(), size_t(n))))
                    break;
            }
            src.shutdown();
            dst.shutdown();
        }

        void _handle(int fd)
        {
            sihd::net::Socket client(fd);
            std::string buf;
            std::vector<char> tmp(4096);
            while (buf.find("\r\n\r\n") == std::string::npos)
            {
                ssize_t n = client.receive(tmp.data(), tmp.size());
                if (n <= 0)
                    return;
                buf.append(tmp.data(), size_t(n));
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
                client.send_all(std::string_view("HTTP/1.1 405 Method Not Allowed\r\n\r\n"));
                return;
            }
            if (!_auth_ok(proxy_auth))
            {
                ++n_407;
                client.send_all(std::string_view(
                    "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"test\"\r\n\r\n"));
                return;
            }

            auto colon = target.rfind(':');
            std::string host = target.substr(0, colon);
            int port = std::stoi(target.substr(colon + 1));
            sihd::net::IpAddr resolved = sihd::net::dns::find(host);
            sihd::net::IpAddr up_addr(resolved.empty() ? host : resolved.str(), port);
            sihd::net::Socket upstream;
            if (!upstream.open(AF_INET, SOCK_STREAM, IPPROTO_TCP) || !upstream.connect(up_addr))
            {
                client.send_all(std::string_view("HTTP/1.1 502 Bad Gateway\r\n\r\n"));
                return;
            }
            ++n_200;
            client.send_all(std::string_view("HTTP/1.1 200 Connection Established\r\n\r\n"));
            if (!rest.empty())
                upstream.send_all(sihd::util::ArrCharView(rest.data(), rest.size()));

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
