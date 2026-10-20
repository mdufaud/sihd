#ifndef __HTTP_TEST_HELPERS_HPP__
#define __HTTP_TEST_HELPERS_HPP__

#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/http/HttpResponse.hpp>
#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/IWebsocketHandler.hpp>
#include <sihd/http/Multipart.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/WriteProtocol.hpp>
#include <sihd/http/request.hpp>
#include <sihd/json/Json.hpp>
#include <sihd/net/IpAddr.hpp>
#include <sihd/net/Socket.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/net/dns.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/Worker.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/time.hpp>

namespace test
{

// concurrent gtest shards (jtest=N) share the loopback: each shard gets its own port range
inline int shard_port(int port)
{
    const char *shard = std::getenv("GTEST_SHARD_INDEX");
    return shard == nullptr ? port : port + std::atoi(shard) * 100;
}

inline bool is_digit_at(std::string_view text, size_t idx)
{
    return std::isdigit(static_cast<unsigned char>(text[idx])) != 0;
}

inline bool is_port_at(std::string_view text, size_t i)
{
    if (text[i] != ':' || text.substr(i + 1, 2) != "30" || i + 5 > text.size())
        return false;
    return is_digit_at(text, i + 3) && is_digit_at(text, i + 4) && (i + 5 == text.size() || !is_digit_at(text, i + 5));
}

// shifts every ":30xx" port of a url or header
inline std::string shard_url(std::string_view text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i)
    {
        out += text[i];
        if (is_port_at(text, i))
        {
            out += std::to_string(shard_port(std::atoi(std::string(text.substr(i + 1, 4)).c_str())));
            i += 4;
        }
    }
    return out;
}

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

template <typename Server>
struct BasicServerScope
{
        Server server;
        sihd::util::Worker worker;

        BasicServerScope():
            worker([this] {
                server.start();
                return true;
            })
        {
        }

        void start(int port = shard_port(3001))
        {
            server.set_port(port);
            ASSERT_TRUE(worker.start_sync_worker("test-server"));
            ASSERT_TRUE(server.wait_ready(std::chrono::milliseconds(500)));
        }

        void stop()
        {
            server.set_service_wait_stop(true);
            server.stop();
        }

        ~BasicServerScope() { stop(); }
};

struct ServerScope: BasicServerScope<FeatureHttpServer>
{
};

inline std::function<void(const sihd::http::HttpRequest &, sihd::http::HttpResponse &)>
    redirect_to(uint32_t status, std::string location)
{
    return [status, location = std::move(location)](const sihd::http::HttpRequest &, sihd::http::HttpResponse & resp) {
        resp.set_status(status);
        resp.http_header().set_header("location", location);
    };
}

inline void allow_local_targets(sihd::http::Navigator & nav)
{
    nav.clear_proxy();
    nav.set_ssrf_guard(false);
}

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

struct SimpleServerScope: BasicServerScope<SimpleHttpServer>
{
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

// raw request client: the http client cannot send a chunked body nor reuse a refused connection
class RawHttpClient
{
    public:
        bool connect(int port = shard_port(3001))
        {
            _client.set_poll_timeout(2000);
            return _client.open_and_connect("127.0.0.1", port).has_value();
        }

        bool send(std::string_view request)
        {
            return _client.send_all(sihd::util::ArrCharView(request.data(), request.size())).has_value();
        }

        bool connected() const { return _client.connected(); }

        std::optional<sihd::http::HttpResponse> read_response()
        {
            const size_t head_size = this->_read_head();
            if (head_size == 0)
                return std::nullopt;
            const size_t body_size = this->_announced_size(_buffer.substr(0, head_size));
            while (_buffer.size() < head_size + body_size)
            {
                if (this->_read_more() == false)
                    break;
            }
            auto response = sihd::http::HttpResponse::from_string(_buffer.substr(0, head_size + body_size));
            _buffer.erase(0, head_size + body_size);
            if (!response)
                return std::nullopt;
            return std::move(*response);
        }

        bool wait_closed()
        {
            while (this->_read_more())
                ;
            return _client.connected() == false;
        }

        void close() { (void)_client.close(); }

    private:
        size_t _read_head()
        {
            size_t head_size = _buffer.find("\r\n\r\n");
            while (head_size == std::string::npos)
            {
                if (this->_read_more() == false)
                    return 0;
                head_size = _buffer.find("\r\n\r\n");
            }
            return head_size + 4;
        }

        bool _read_more()
        {
            if (_client.poll() == false)
                return false;
            std::array<char, 4096> buf;
            const auto read = _client.receive(buf.data(), buf.size());
            if (!read || read.value() == 0)
                return false;
            _buffer.append(buf.data(), read.value());
            return true;
        }

        static size_t _announced_size(std::string_view head)
        {
            for (const std::string_view line : sihd::util::str::split(head, "\r\n"))
            {
                const auto [name, value] = sihd::util::str::split_pair_view(line, ":");
                if (sihd::util::str::iequals(sihd::util::str::trim(name), "content-length"))
                    return sihd::util::str::convert_from_string<size_t>(sihd::util::str::trim(value)).value_or(0);
            }
            return 0;
        }

        sihd::net::TcpClient _client {"raw-http-client"};
        std::string _buffer;
};

class HeadCaptureServer
{
    public:
        explicit HeadCaptureServer(size_t body_size = 0):
            _body_size(body_size),
            _worker([this] {
                this->_serve();
                return true;
            })
        {
        }

        bool start()
        {
            if (!_listener.open(AF_INET, SOCK_STREAM, 0) || !_listener.set_reuseaddr(true)
                || !_listener.bind(sihd::net::IpAddr::localhost(0)) || !_listener.listen(1))
                return false;
            _port = _listener.local_port().value_or(-1);
            return _port > 0 && _worker.start_sync_worker("head-capture");
        }

        int port() const { return _port; }

        const std::string & finish()
        {
            (void)_worker.stop_worker();
            return _head;
        }

    private:
        void _serve()
        {
            sihd::net::Socket conn(_listener.accept(5000).value_or(-1));
            if (!conn.is_open() || !conn.set_recv_timeout(5000).has_value())
                return;
            std::string received;
            std::array<char, 65536> buffer;
            size_t head_end = std::string::npos;
            while (head_end == std::string::npos || received.size() < head_end + 4 + _body_size)
            {
                const auto size = conn.receive(buffer.data(), buffer.size());
                if (!size || *size == 0)
                    return;
                received.append(buffer.data(), *size);
                if (head_end != std::string::npos)
                    continue;
                head_end = received.find("\r\n\r\n");
                if (head_end == std::string::npos)
                    continue;
                _head = received.substr(0, head_end);
                constexpr std::string_view proceed = "HTTP/1.1 100 Continue\r\n\r\n";
                if (_head.find("Expect: 100-continue") != std::string::npos
                    && !conn.send_all({proceed.data(), proceed.size()}).has_value())
                    return;
            }
            constexpr std::string_view reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
            (void)conn.send_all({reply.data(), reply.size()});
        }

        size_t _body_size;
        int _port = 0;
        std::string _head;
        sihd::net::Socket _listener;
        sihd::util::Worker _worker;
};

// Minimal WebSocket client for automated testing
class SimpleWsClient
{
    public:
        SimpleWsClient() { _client.set_poll_timeout(2000); }

        bool connect(int port) { return _client.open_and_connect("127.0.0.1", port).has_value(); }

        // Perform the HTTP -> WebSocket upgrade handshake.
        bool handshake(const char *protocol) { return this->upgrade("/", protocol) == switching_protocols; }

        std::string upgrade(std::string_view path,
                            std::string_view protocol,
                            std::string_view extra_headers = {},
                            std::string_view host = "localhost")
        {
            // Fixed 16-byte WebSocket key (base64 of 0x01..0x10).
            const char *key = "AQIDBAUGBwgJCgsMDQ4PEA==";
            std::string req = fmt::format("GET {} HTTP/1.1\r\n"
                                          "Host: {}\r\n"
                                          "Upgrade: websocket\r\n"
                                          "Connection: Upgrade\r\n"
                                          "Sec-WebSocket-Key: {}\r\n"
                                          "Sec-WebSocket-Version: 13\r\n",
                                          path,
                                          host,
                                          key);
            if (protocol.empty() == false)
                req += fmt::format("Sec-WebSocket-Protocol: {}\r\n", protocol);
            req += extra_headers;
            req += "\r\n";

            if (_client.send_all(sihd::util::ArrCharView(req.data(), req.size())).has_value() == false)
                return {};

            std::string response;
            std::array<uint8_t, 1> received_byte;
            while (response.find("\r\n\r\n") == std::string::npos)
            {
                if (_recv_exact(received_byte.data(), received_byte.size()) == false)
                    return {};
                response += static_cast<char>(received_byte[0]);
            }
            return response.substr(0, response.find("\r\n"));
        }

        static constexpr std::string_view switching_protocols = "HTTP/1.1 101 Switching Protocols";

        // Send a masked WebSocket text frame (len <= 125).
        bool send_text(std::string_view text)
        {
            std::vector<uint8_t> frame;
            frame.push_back(0x81);                        // FIN + opcode=text
            frame.push_back(0x80 | (uint8_t)text.size()); // MASK + len
            constexpr std::array<uint8_t, 4> mask {0x11, 0x22, 0x33, 0x44};
            frame.insert(frame.end(), mask.begin(), mask.end());
            for (size_t i = 0; i < text.size(); ++i)
                frame.push_back((uint8_t)text[i] ^ mask[i % 4]);
            return _client.send_all(sihd::util::ArrCharView((const char *)frame.data(), frame.size())).has_value();
        }

        // Receive one unmasked text frame from the server.
        std::optional<std::string> recv_text()
        {
            std::array<uint8_t, 2> header;
            if (_recv_exact(header.data(), header.size()) == false)
                return {};

            uint8_t opcode = header[0] & 0x0F;
            if (opcode != 0x01) // not a text frame
                return {};

            size_t len = header[1] & 0x7F;
            if (len == 126)
            {
                std::array<uint8_t, 2> extension;
                if (_recv_exact(extension.data(), extension.size()) == false)
                    return {};
                len = ((size_t)extension[0] << 8) | extension[1];
            }

            std::string payload(len, '\0');
            if (len > 0 && _recv_exact((uint8_t *)payload.data(), len) == false)
                return {};
            return payload;
        }

        // Send a masked WebSocket close frame with no payload.
        void send_close()
        {
            constexpr std::array<uint8_t, 6> close_frame {0x88, 0x80, 0x00, 0x00, 0x00, 0x00};
            (void)_client.send_all(sihd::util::ArrCharView((const char *)close_frame.data(), close_frame.size()));
        }

    private:
        bool _recv_exact(uint8_t *buf, size_t count)
        {
            size_t received = 0;
            while (received < count)
            {
                if (_client.poll() == false)
                    return false;
                const auto received_now = _client.receive(buf + received, count - received);
                if (!received_now || received_now.value() == 0)
                    return false;
                received += received_now.value();
            }
            return true;
        }

        sihd::net::TcpClient _client {"ws-client"};
};

} // namespace test

#endif
