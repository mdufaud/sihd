#include <chrono>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/Navigator.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/WebsocketHandler.hpp>
#include <sihd/http/request.hpp>
#include <sihd/json/Json.hpp>
#include <sihd/net/TcpClient.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/SigWatcher.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/Waitable.hpp>
#include <sihd/util/build.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>
#include <sihd/util/time.hpp>

#include "http_test_helpers.hpp"
#include "sihd/util/LogInfo.hpp"
#include "sihd/util/LoggerFilter.hpp"
#include "sihd/util/LoggerManager.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::http;
using namespace sihd::util;
using namespace sihd::sys;
class TestHttpServer: public ::testing::Test
{
    protected:
        TestHttpServer() { sihd::util::LoggerManager::stream(); }

        virtual ~TestHttpServer() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        std::string _cwd;
        std::string _base_test_dir;
};

class SimpleHttpServer: public sihd::http::HttpServer,
                        public sihd::http::IWebsocketHandler
{
    public:
        SimpleHttpServer(): HttpServer("http-server-test")
        {
            // HttpServer protected call
            this->add_websocket("proto-two", this);
            _webservice = this->add_child<WebService>("web");
            this->setup_webservice_entry_points();
        }

        ~SimpleHttpServer() = default;

        void setup_webservice_entry_points()
        {
            _webservice->set_entry_point("some_get", [this](const HttpRequest & req, HttpResponse & resp) {
                SIHD_LOG(info, "{} request received", req.type_str());
                resp.set_plain_content("hello get world");
                ++_nget;
            });

            _webservice->set_entry_point(
                "some_post",
                [this](const HttpRequest & req, HttpResponse & resp) {
                    SIHD_LOG(info, "{} request received", req.type_str());
                    if (req.has_content())
                    {
                        _post_content = req.content().str();
                        SIHD_LOG(info, "Received POST body: {}", _post_content);
                        resp.set_status(HttpStatus::Ok);
                        ++_npost;
                    }
                    else
                        resp.set_status(HttpStatus::BadRequest);
                },
                HttpRequest::Post);

            _webservice->set_entry_point(
                "some_delete",
                [this](const HttpRequest & req, HttpResponse & resp) {
                    SIHD_LOG(info, "{} request received", req.type_str());
                    resp.set_status(HttpStatus::Ok);
                    resp.set_json_content({"hello", "world"});
                    ++_ndelete;
                },
                HttpRequest::Delete);

            _webservice->set_entry_point(
                "some_put",
                [this](const HttpRequest & req, HttpResponse & resp) {
                    SIHD_LOG(info, "{} request received", req.type_str());
                    if (req.has_content())
                    {
                        _put_content = req.content().str();
                        SIHD_LOG(info, "Received PUT body: {}", _put_content);
                        resp.set_status(HttpStatus::Ok);
                        ++_nput;
                    }
                    else
                        resp.set_status(HttpStatus::BadRequest);
                },
                HttpRequest::Put);
        }

        // IWebsocketHandler

        void on_open(std::string_view protocol_name) override
        {
            SIHD_LOG(debug, "Opened websocket of protocol: {}", protocol_name);
            {
                auto l = _ws_waitable.guard();
                ++_nopen;
            }
            _ws_waitable.notify_all();
        };

        bool on_read(const sihd::util::ArrChar & array) override
        {
            SIHD_LOG(debug, "Read from client websocket: {}", array.str());
            {
                auto l = _ws_waitable.guard();
                _client_wrote = true;
                ++_nread;
            }
            _ws_waitable.notify_all();
            return true;
        };

        bool on_write(sihd::util::ArrChar & array, WriteProtocol & protocol) override
        {
            auto l = _ws_waitable.guard();
            if (_client_wrote)
            {
                ++_nwrite;
                _client_wrote = false;

                const char hw[] = "hello world";
                array.from(hw);

                protocol = WriteProtocol::Text;
                SIHD_LOG(debug, "Wrote back to client websocket: {}", hw);
            }
            return true;
        }

        void on_close() override
        {
            SIHD_LOG(debug, "Closed websocket");
            {
                auto l = _ws_waitable.guard();
                ++_nclosed;
            }
            _ws_waitable.notify_all();
            this->request_stop();
        }

        void on_peer_close(uint16_t code, std::string_view reason) override
        {
            SIHD_LOG(debug, "Peer closed websocket: code={} reason={}", code, reason);
        }

        bool wait_for_open(sihd::util::Duration timeout = sihd::util::Duration(sihd::util::time::sec(2)))
        {
            return _ws_waitable.wait_for(timeout, [this] { return _nopen > 0; });
        }

        bool wait_for_close(sihd::util::Duration timeout = sihd::util::Duration(sihd::util::time::sec(2)))
        {
            return _ws_waitable.wait_for(timeout, [this] { return _nclosed > 0; });
        }

        // websocket
        Waitable _ws_waitable;
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
        std::string _put_content;
};

TEST_F(TestHttpServer, test_httpserver_auto)
{
    SimpleHttpServer server;
    server.set_root_dir("test/resources/mount_point");
    server.set_port(3000);

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("server-thread"));
    ASSERT_TRUE(server.wait_ready(std::chrono::milliseconds(500)));

    TmpDir tmpdir;
    std::string tmpfile_path = fs::combine(tmpdir.path(), "test_file.txt");
    fs::write(tmpfile_path, "hello put world");

    RequestOptions options;
    options.proxy = "";
    // launch all requests async
    auto get_future = sihd::http::async_get("localhost:3000/web/some_get", options);
    auto post_future = sihd::http::async_post("localhost:3000/web/some_post", "hello world", options);
    auto put_future = sihd::http::async_put("localhost:3000/web/some_put", tmpfile_path, options);
    auto del_future = sihd::http::async_del("localhost:3000/web/some_delete", options);

    // wait all
    auto get_resp = get_future.get();
    auto post_resp = post_future.get();
    auto put_resp = put_future.get();
    auto del_resp = del_future.get();

    server.set_service_wait_stop(true);
    server.stop();

    // verify GET
    ASSERT_TRUE(get_resp.has_value());
    EXPECT_EQ(get_resp->status(), HttpStatus::Ok);
    EXPECT_EQ(get_resp->content().cpp_str(), "hello get world");
    EXPECT_EQ(server._nget, 1);

    // verify POST
    ASSERT_TRUE(post_resp.has_value());
    EXPECT_EQ(post_resp->status(), HttpStatus::Ok);
    EXPECT_EQ(server._npost, 1);
    EXPECT_EQ(server._post_content, "hello world");

    // verify PUT
    ASSERT_TRUE(put_resp.has_value());
    EXPECT_EQ(put_resp->status(), HttpStatus::Ok);
    EXPECT_EQ(server._nput, 1);
    EXPECT_EQ(server._put_content, "hello put world");

    // verify DELETE
    ASSERT_TRUE(del_resp.has_value());
    EXPECT_EQ(del_resp->status(), HttpStatus::Ok);
    EXPECT_EQ(server._ndelete, 1);
}

TEST_F(TestHttpServer, test_httpserver_websockets)
{
    if (sihd::util::term::is_interactive() == false)
        GTEST_SKIP_("requires interaction");

    SimpleHttpServer server;
    server.set_root_dir("test/resources/mount_point");
    server.set_port(3000);

    SigWatcher watcher({SIGINT}, [&server]([[maybe_unused]] int sig) {
        SIHD_LOG(info, "Stopping http server...");
        server.stop();
        SIHD_LOG(info, "Stopped http server");
    });

    SIHD_LOG(info, "=========================================================");
    SIHD_LOG(info, "Open web browser at http://localhost:3000 then close it");
    SIHD_LOG(info, "=========================================================");

    server.start();
}

// Minimal WebSocket client for automated testing
class SimpleWsClient
{
    public:
        SimpleWsClient() { _client.set_poll_timeout(2000); }

        bool connect(int port) { return _client.open_and_connect("127.0.0.1", port); }

        // Perform the HTTP -> WebSocket upgrade handshake.
        bool handshake(const char *protocol)
        {
            // Fixed 16-byte WebSocket key (base64 of 0x01..0x10).
            const char *key = "AQIDBAUGBwgJCgsMDQ4PEA==";
            std::string req = "GET / HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Key: ";
            req += key;
            req += "\r\nSec-WebSocket-Version: 13\r\n"
                   "Sec-WebSocket-Protocol: ";
            req += protocol;
            req += "\r\n\r\n";

            if (_client.send_all(sihd::util::ArrCharView(req.data(), req.size())) == false)
                return false;

            std::string response;
            char buf[1];
            while (response.find("\r\n\r\n") == std::string::npos)
            {
                if (_recv_exact((uint8_t *)buf, 1) == false)
                    return false;
                response += buf[0];
            }
            return response.find(" 101 ") != std::string::npos;
        }

        // Send a masked WebSocket text frame (len <= 125).
        bool send_text(std::string_view text)
        {
            std::vector<uint8_t> frame;
            frame.push_back(0x81);                        // FIN + opcode=text
            frame.push_back(0x80 | (uint8_t)text.size()); // MASK + len
            const uint8_t mask[4] = {0x11, 0x22, 0x33, 0x44};
            frame.insert(frame.end(), mask, mask + 4);
            for (size_t i = 0; i < text.size(); ++i)
                frame.push_back((uint8_t)text[i] ^ mask[i % 4]);
            return _client.send_all(sihd::util::ArrCharView((const char *)frame.data(), frame.size()));
        }

        // Receive one unmasked text frame from the server.
        std::optional<std::string> recv_text()
        {
            uint8_t header[2];
            if (_recv_exact(header, 2) == false)
                return {};

            uint8_t opcode = header[0] & 0x0F;
            if (opcode != 0x01) // not a text frame
                return {};

            size_t len = header[1] & 0x7F;
            if (len == 126)
            {
                uint8_t ext[2];
                if (_recv_exact(ext, 2) == false)
                    return {};
                len = ((size_t)ext[0] << 8) | ext[1];
            }

            std::string payload(len, '\0');
            if (len > 0 && _recv_exact((uint8_t *)payload.data(), len) == false)
                return {};
            return payload;
        }

        // Send a masked WebSocket close frame with no payload.
        void send_close()
        {
            const uint8_t frame[] = {0x88, 0x80, 0x00, 0x00, 0x00, 0x00};
            _client.send_all(sihd::util::ArrCharView((const char *)frame, sizeof(frame)));
        }

    private:
        bool _recv_exact(uint8_t *buf, size_t n)
        {
            size_t received = 0;
            while (received < n)
            {
                if (_client.poll() == false)
                    return false;
                const ssize_t r = _client.receive(buf + received, n - received);
                if (r <= 0)
                    return false;
                received += (size_t)r;
            }
            return true;
        }

        sihd::net::TcpClient _client {"ws-client"};
};

TEST_F(TestHttpServer, test_httpserver_websockets_auto)
{
    SimpleHttpServer server;
    server.set_service_wait_stop(true);
    server.set_root_dir("test/resources/mount_point");
    server.set_port(3002);

    Worker worker([&server] {
        server.start();
        return true;
    });
    ASSERT_TRUE(worker.start_sync_worker("ws-auto-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::milliseconds(500)));

    SimpleWsClient client;
    ASSERT_TRUE(client.connect(3002));
    ASSERT_TRUE(client.handshake("proto-two"));
    ASSERT_TRUE(server.wait_for_open());

    ASSERT_TRUE(client.send_text("hello from client"));

    auto reply = client.recv_text();
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(*reply, "hello world");

    client.send_close();
    ASSERT_TRUE(server.wait_for_close());
    server.stop();

    EXPECT_EQ(server._nopen, 1);
    EXPECT_EQ(server._nread, 1);
    EXPECT_EQ(server._nwrite, 1);
    EXPECT_GE(server._nclosed, 1);
}

TEST_F(TestHttpServer, test_routing_params)
{
    ServerScope scope;

    std::string captured_id;
    std::string captured_action;

    scope.server._webservice->set_entry_point("users/{id}", [&](const HttpRequest & req, HttpResponse & resp) {
        auto id = req.path_param("id");
        captured_id = id.value_or("");
        resp.set_plain_content(fmt::format("user:{}", captured_id));
    });

    scope.server._webservice->set_entry_point("users/{id}/{action}", [&](const HttpRequest & req, HttpResponse & resp) {
        captured_id = std::string(req.path_param("id").value_or(""));
        captured_action = std::string(req.path_param("action").value_or(""));
        resp.set_plain_content(fmt::format("{}:{}", captured_id, captured_action));
    });

    scope.start();

    // single param
    auto resp = sihd::http::get("localhost:3001/api/users/42");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "user:42");
    EXPECT_EQ(captured_id, "42");

    // multi params — more specific route matches
    resp = sihd::http::get("localhost:3001/api/users/7/edit");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->content().cpp_str(), "7:edit");
    EXPECT_EQ(captured_id, "7");
    EXPECT_EQ(captured_action, "edit");
}

TEST_F(TestHttpServer, test_routing_catchall)
{
    ServerScope scope;

    std::string captured_path;

    scope.server._webservice->set_entry_point("files/{path...}", [&](const HttpRequest & req, HttpResponse & resp) {
        captured_path = std::string(req.path_param("path").value_or(""));
        resp.set_plain_content(captured_path);
    });

    scope.start();

    auto resp = sihd::http::get("localhost:3001/api/files/dir/subdir/file.txt");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(captured_path, "dir/subdir/file.txt");
}

TEST_F(TestHttpServer, test_http_filter)
{
    ServerScope scope;
    TestFilter filter;
    filter.blocked_uri = "/api/secret";

    scope.server.set_http_filter(&filter);
    scope.server._webservice->set_entry_point("public", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
    });
    scope.server._webservice->set_entry_point("secret", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("hidden");
    });

    scope.start();

    // allowed request
    auto resp = sihd::http::get("localhost:3001/api/public");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "ok");
    EXPECT_GE(filter.filter_calls, 1);

    // blocked request → 403
    resp = sihd::http::get("localhost:3001/api/secret");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Forbidden);
}

TEST_F(TestHttpServer, test_basic_auth)
{
    ServerScope scope;
    TestAuth auth;

    scope.server.set_authenticator(&auth);
    scope.server._webservice->set_entry_point("protected", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("welcome");
    });

    scope.server._webservice->set_entry_point(
        "protected_post",
        [](const HttpRequest & req, HttpResponse & resp) {
            resp.set_plain_content(fmt::format("posted:{}", req.content().cpp_str()));
        },
        HttpRequest::Post);

    scope.start();

    // no credentials → 401 for GET, POST and DELETE
    auto no_auth_get = sihd::http::async_get("localhost:3001/api/protected");
    auto no_auth_post = sihd::http::async_post("localhost:3001/api/protected_post", "data");
    auto no_auth_del = sihd::http::async_del("localhost:3001/api/protected");

    auto resp_get = no_auth_get.get();
    auto resp_post = no_auth_post.get();
    auto resp_del = no_auth_del.get();

    ASSERT_TRUE(resp_get.has_value());
    EXPECT_EQ(resp_get->status(), HttpStatus::Unauthorized);
    ASSERT_TRUE(resp_post.has_value());
    EXPECT_EQ(resp_post->status(), HttpStatus::Unauthorized);
    ASSERT_TRUE(resp_del.has_value());
    EXPECT_EQ(resp_del->status(), HttpStatus::Unauthorized);

    // wrong credentials → 401
    RequestOptions bad_creds;
    bad_creds.username = "admin";
    bad_creds.password = "wrong";
    auto resp = sihd::http::get("localhost:3001/api/protected", bad_creds);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    // wrong user → 401
    RequestOptions bad_user;
    bad_user.username = "nobody";
    bad_user.password = "secret";
    resp = sihd::http::get("localhost:3001/api/protected", bad_user);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    // correct credentials → 200
    RequestOptions good_creds;
    good_creds.username = "admin";
    good_creds.password = "secret";

    auto auth_get = sihd::http::async_get("localhost:3001/api/protected", good_creds);
    auto auth_post = sihd::http::async_post("localhost:3001/api/protected_post", "hello", good_creds);

    auto good_get = auth_get.get();
    auto good_post = auth_post.get();

    ASSERT_TRUE(good_get.has_value());
    EXPECT_EQ(good_get->status(), HttpStatus::Ok);
    EXPECT_EQ(good_get->content().cpp_str(), "welcome");

    ASSERT_TRUE(good_post.has_value());
    EXPECT_EQ(good_post->status(), HttpStatus::Ok);
    EXPECT_EQ(good_post->content().cpp_str(), "posted:hello");
}

TEST_F(TestHttpServer, test_token_auth)
{
    ServerScope scope;
    TestAuth auth;

    scope.server.set_authenticator(&auth);
    scope.server._webservice->set_entry_point("secure", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("token-ok");
    });
    scope.server._webservice->set_entry_point(
        "secure_post",
        [](const HttpRequest & req, HttpResponse & resp) {
            resp.set_plain_content(fmt::format("posted:{}", req.content().cpp_str()));
        },
        HttpRequest::Post);

    scope.start();

    // no token → 401
    auto resp = sihd::http::get("localhost:3001/api/secure");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    // wrong token → 401
    RequestOptions bad_token;
    bad_token.token = "invalid-token";
    resp = sihd::http::get("localhost:3001/api/secure", bad_token);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    // correct token → 200
    RequestOptions good_token;
    good_token.token = "my-secret-token-123";
    resp = sihd::http::get("localhost:3001/api/secure", good_token);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "token-ok");

    // token works for POST too
    auto post_resp = sihd::http::post("localhost:3001/api/secure_post", "data", good_token);
    ASSERT_TRUE(post_resp.has_value());
    EXPECT_EQ(post_resp->status(), HttpStatus::Ok);
    EXPECT_EQ(post_resp->content().cpp_str(), "posted:data");

    // basic auth credentials should also still work
    RequestOptions basic_creds;
    basic_creds.username = "admin";
    basic_creds.password = "secret";
    resp = sihd::http::get("localhost:3001/api/secure", basic_creds);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "token-ok");
}

TEST_F(TestHttpServer, test_streaming)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("stream", [](const HttpRequest &, HttpResponse & resp) {
        int chunk_num = 0;
        resp.set_stream_provider([chunk_num](ArrByte & chunk) mutable -> bool {
            std::string data = fmt::format("chunk{}", chunk_num);
            chunk.resize(data.size());
            chunk.copy_from_bytes(data.data(), data.size());
            ++chunk_num;
            return chunk_num < 3; // 3 chunks: chunk0, chunk1, chunk2
        });
        resp.set_status(HttpStatus::Ok);
        resp.set_content_type("text/plain");
    });

    scope.start();

    auto resp = sihd::http::get("localhost:3001/api/stream");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    // chunked transfer: all chunks concatenated
    std::string body = resp->content().cpp_str();
    EXPECT_TRUE(body.find("chunk0") != std::string::npos);
    EXPECT_TRUE(body.find("chunk1") != std::string::npos);
    EXPECT_TRUE(body.find("chunk2") != std::string::npos);
}

TEST_F(TestHttpServer, test_streaming_file_response)
{
    ServerScope scope;
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    const std::string path = fs::combine(tmp.path(), "stream.bin");
    std::string content(4 * 1024 * 1024, '\0');
    for (size_t i = 0; i < content.size(); ++i)
        content[i] = (char)(i * 31 % 251);
    ASSERT_TRUE(fs::write(path, content, false, true));

    const std::string empty_path = fs::combine(tmp.path(), "empty.bin");
    ASSERT_TRUE(fs::write(empty_path, "", false, true));

    scope.server._webservice->set_entry_point("file", [&path](const HttpRequest &, HttpResponse & resp) {
        ASSERT_TRUE(resp.set_file_content(path));
    });
    scope.server._webservice->set_entry_point("empty", [&empty_path](const HttpRequest &, HttpResponse & resp) {
        ASSERT_TRUE(resp.set_file_content(empty_path));
    });

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    auto first = nav.get("localhost:3001/api/file");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->status(), HttpStatus::Ok);
    EXPECT_EQ(first->http_header().content_length(), content.size());
    EXPECT_EQ(first->content().cpp_str(), content);

    auto second = nav.get("localhost:3001/api/file");
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->content().cpp_str(), content);

    auto empty = nav.get("localhost:3001/api/empty");
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(empty->status(), HttpStatus::Ok);
    EXPECT_EQ(empty->http_header().content_length(), 0u);
    EXPECT_TRUE(empty->content().cpp_str().empty());
}

TEST_F(TestHttpServer, test_streaming_upload_to_file)
{
    ServerScope scope;
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    std::string body(3 * 1024 * 1024, '\0');
    for (size_t i = 0; i < body.size(); ++i)
        body[i] = (char)(i * 17 % 251);

    std::string upload_path;
    int hits = 0;

    BodyStreamFile upload_stream;
    upload_stream.set_path_provider([&upload_path, &tmp](const HttpRequest &) {
        upload_path = fs::combine(tmp.path(), "upload.bin");
        return upload_path;
    });
    scope.server._webservice->set_body_stream(&upload_stream);
    scope.server._webservice->set_entry_point(
        "upload",
        [&hits](const HttpRequest & req, HttpResponse & resp) {
            ++hits;
            EXPECT_FALSE(req.has_content());
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    auto resp = sihd::http::post("localhost:3001/api/upload", body);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(hits, 1);

    sihd::sys::File uploaded;
    ASSERT_TRUE(uploaded.open(upload_path, "rb"));
    ASSERT_EQ(uploaded.file_size(), (long)body.size());
    std::string received(body.size(), '\0');
    ASSERT_EQ(uploaded.read(received.data(), received.size()), (ssize_t)body.size());
    EXPECT_EQ(received, body);
}

TEST_F(TestHttpServer, test_streaming_upload_chunks)
{
    ServerScope scope;

    std::string body(1024 * 1024, '\0');
    for (size_t i = 0; i < body.size(); ++i)
        body[i] = (char)(i * 29 % 251);

    int chunks = 0;
    size_t streamed = 0;
    size_t announced = 0;

    BodyStreamFunc chunk_stream([&](const HttpRequest & req, sihd::util::ArrCharView chunk) {
        ++chunks;
        streamed += chunk.size();
        announced = req.http_header().content_length().value_or(0);
        return true;
    });
    scope.server._webservice->set_body_stream(&chunk_stream);
    scope.server._webservice->set_entry_point(
        "upload",
        [](const HttpRequest & req, HttpResponse & resp) {
            EXPECT_FALSE(req.has_content());
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    auto resp = sihd::http::post("localhost:3001/api/upload", body);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_GT(chunks, 1);
    EXPECT_EQ(streamed, body.size());
    EXPECT_EQ(announced, body.size());
}

TEST_F(TestHttpServer, test_streaming_upload_stream_failure)
{
    ServerScope scope;
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    const std::string bad_path = fs::combine({tmp.path(), "missing_dir", "upload.bin"});
    int hits = 0;

    BodyStreamFile bad_stream(bad_path);
    scope.server._webservice->set_body_stream(&bad_stream);
    scope.server._webservice->set_entry_point(
        "upload",
        [&hits](const HttpRequest &, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    auto resp = sihd::http::post("localhost:3001/api/upload", std::string(4096, 'x'));
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::InternalServerError);
    EXPECT_EQ(hits, 0);
    EXPECT_FALSE(fs::is_file(bad_path));
}

TEST_F(TestHttpServer, test_streaming_upload_stream_not_set)
{
    HttpRequest request("/upload", HttpRequest::Post);

    BodyStreamFile file_stream;
    EXPECT_THROW(file_stream.on_chunk(request, {}), std::logic_error);

    BodyStreamFunc func_stream;
    EXPECT_THROW(func_stream.on_chunk(request, {}), std::logic_error);
}

TEST_F(TestHttpServer, test_streaming_upload_multipart)
{
    ServerScope scope;
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    const std::string file_path = fs::combine(tmp.path(), "doc.bin");
    std::string content(2 * 1024 * 1024, '\0');
    for (size_t i = 0; i < content.size(); ++i)
        content[i] = (char)(i * 13 % 251);
    ASSERT_TRUE(fs::write(file_path, content, false, true));

    std::string seen_field;
    std::string seen_filename;
    std::string seen_data;

    scope.server._webservice->set_entry_point(
        "form",
        [&](const HttpRequest & req, HttpResponse & resp) {
            if (const Multipart *multipart = req.multipart(); multipart != nullptr)
            {
                seen_field = multipart->value("note").value_or("");
                if (const Multipart::Part *part = multipart->file("doc"); part != nullptr)
                {
                    seen_filename = part->filename;
                    seen_data = part->data;
                }
            }
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    Multipart form;
    form.add_field("note", "hello");
    form.add_file("doc", file_path, "doc.bin", "application/octet-stream");

    auto resp = nav.post_multipart("localhost:3001/api/form", form);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(seen_field, "hello");
    EXPECT_EQ(seen_filename, "doc.bin");
    EXPECT_EQ(seen_data, content);
}

TEST_F(TestHttpServer, test_streaming_upload_multipart_rejected)
{
    ServerScope scope;
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    int hits = 0;
    BodyStreamFile upload_stream;
    upload_stream.set_path_provider([&tmp](const HttpRequest &) { return fs::combine(tmp.path(), "upload.bin"); });
    scope.server._webservice->set_body_stream(&upload_stream);
    scope.server._webservice->set_entry_point(
        "form",
        [&hits](const HttpRequest &, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    Navigator nav;
    nav.clear_proxy();

    Multipart form;
    form.add_field("note", "hello");

    // the webservice streams its body: multipart cannot also be parsed
    auto resp = nav.post_multipart("localhost:3001/api/form", form);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::InternalServerError);
    EXPECT_EQ(hits, 0);
    EXPECT_FALSE(fs::is_file(fs::combine(tmp.path(), "upload.bin")));
}

TEST_F(TestHttpServer, test_max_request_size)
{
    ServerScope scope;

    int hits = 0;
    scope.server.set_max_request_size(1024);
    scope.server._webservice->set_entry_point(
        "small",
        [&hits](const HttpRequest &, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    auto too_big = sihd::http::post("localhost:3001/api/small", std::string(4096, 'x'));
    ASSERT_TRUE(too_big.has_value());
    EXPECT_EQ(too_big->status(), HttpStatus::PayloadTooLarge);
    EXPECT_EQ(hits, 0);

    auto ok = sihd::http::post("localhost:3001/api/small", std::string(512, 'x'));
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(ok->status(), HttpStatus::Ok);
    EXPECT_EQ(hits, 1);

    // exactly at the limit is still served
    auto at_limit = sihd::http::post("localhost:3001/api/small", std::string(1024, 'x'));
    ASSERT_TRUE(at_limit.has_value());
    EXPECT_EQ(at_limit->status(), HttpStatus::Ok);
    EXPECT_EQ(hits, 2);
}

// raw request client: the http client cannot send a chunked body nor reuse a refused connection
class RawHttpClient
{
    public:
        bool connect(int port = 3001)
        {
            _client.set_poll_timeout(2000);
            return _client.open_and_connect("127.0.0.1", port);
        }

        bool send(std::string_view request) { return _client.send_all(ArrCharView(request.data(), request.size())); }

        bool connected() const { return _client.connected(); }

        std::optional<HttpResponse> read_response()
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
            auto response = HttpResponse::from_string(_buffer.substr(0, head_size + body_size));
            _buffer.erase(0, head_size + body_size);
            return response;
        }

        bool wait_closed()
        {
            while (this->_read_more())
                ;
            return _client.connected() == false;
        }

        void close() { _client.close(); }

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
            char buf[4096];
            const ssize_t read = _client.receive(buf, sizeof(buf));
            if (read <= 0)
                return false;
            _buffer.append(buf, (size_t)read);
            return true;
        }

        static size_t _announced_size(std::string_view head)
        {
            for (const std::string_view line : str::split(head, "\r\n"))
            {
                const auto [name, value] = str::split_pair_view(line, ":");
                if (str::iequals(str::trim(name), "content-length"))
                    return str::convert_from_string<size_t>(str::trim(value)).value_or(0);
            }
            return 0;
        }

        sihd::net::TcpClient _client {"raw-http-client"};
        std::string _buffer;
};

TEST_F(TestHttpServer, test_upload_chunked_not_supported)
{
    ServerScope scope;

    int hits = 0;
    scope.server._webservice->set_entry_point(
        "chunked",
        [&hits](const HttpRequest &, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    RawHttpClient client;
    ASSERT_TRUE(client.connect());

    // the server does not decode chunked bodies, it asks for a length instead
    ASSERT_TRUE(client.send("POST /api/chunked HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "Content-Type: text/plain\r\n"
                            "Transfer-Encoding: chunked\r\n"
                            "\r\n"
                            "5\r\nhello\r\n0\r\n\r\n"));

    auto response = client.read_response();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status(), HttpStatus::LengthRequired);
    EXPECT_EQ(response->http_header().find("connection"), "close");
    // the unreadable body is not left in the connection
    EXPECT_TRUE(client.wait_closed());
    EXPECT_EQ(hits, 0);
}

TEST_F(TestHttpServer, test_request_without_length)
{
    ServerScope scope;

    int hits = 0;
    scope.server._webservice->set_entry_point(
        "no_length",
        [&hits](const HttpRequest &, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    RawHttpClient client;
    ASSERT_TRUE(client.connect());

    ASSERT_TRUE(client.send("POST /api/no_length HTTP/1.1\r\nHost: localhost\r\n\r\n"));

    auto response = client.read_response();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status(), HttpStatus::LengthRequired);
    EXPECT_EQ(response->http_header().find("connection"), "close");
    EXPECT_TRUE(client.wait_closed());
    EXPECT_EQ(hits, 0);
}

TEST_F(TestHttpServer, test_unknown_route_with_body)
{
    ServerScope scope;

    int hits = 0;
    scope.server._webservice->set_entry_point(
        "known",
        [&hits](const HttpRequest & req, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content(req.content().cpp_str());
        },
        HttpRequest::Post);

    scope.start();

    RawHttpClient client;
    ASSERT_TRUE(client.connect());

    // a request carrying a body that matches no route must be answered
    ASSERT_TRUE(client.send("POST /api/unknown HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello"));
    auto not_found = client.read_response();
    ASSERT_TRUE(not_found.has_value());
    EXPECT_EQ(not_found->status(), HttpStatus::NotFound);
    EXPECT_EQ(hits, 0);

    // its body was read entirely, the connection serves the next request
    ASSERT_TRUE(client.send("POST /api/known HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello"));
    auto served = client.read_response();
    ASSERT_TRUE(served.has_value());
    EXPECT_EQ(served->status(), HttpStatus::Ok);
    EXPECT_EQ(served->content().cpp_str(), "hello");
    EXPECT_EQ(hits, 1);
}

TEST_F(TestHttpServer, test_streaming_response_keeps_connection)
{
    ServerScope scope;
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    const std::string path = fs::combine(tmp.path(), "stream.bin");
    std::string content(4 * 1024 * 1024, '\0');
    for (size_t i = 0; i < content.size(); ++i)
        content[i] = (char)(i * 31 % 251);
    ASSERT_TRUE(fs::write(path, content, false, true));

    scope.server._webservice->set_entry_point("file", [&path](const HttpRequest &, HttpResponse & resp) {
        ASSERT_TRUE(resp.set_file_content(path));
    });
    scope.start();

    // a raw client reuses the connection without any retry: a closed connection fails here
    RawHttpClient client;
    ASSERT_TRUE(client.connect());
    ASSERT_TRUE(client.send("GET /api/file HTTP/1.1\r\nHost: localhost\r\n\r\n"));

    auto first = client.read_response();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->status(), HttpStatus::Ok);
    EXPECT_EQ(first->http_header().content_length(), content.size());
    EXPECT_EQ(first->content().cpp_str(), content);
    ASSERT_TRUE(client.connected());

    ASSERT_TRUE(client.send("GET /api/file HTTP/1.1\r\nHost: localhost\r\n\r\n"));
    auto second = client.read_response();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->status(), HttpStatus::Ok);
    EXPECT_EQ(second->content().cpp_str(), content);
}

TEST_F(TestHttpServer, test_stream_shorter_than_announced)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point("short", [](const HttpRequest &, HttpResponse & resp) {
        resp.http_header().set_content_length(64);
        resp.set_stream_provider([](ArrByte & chunk) {
            const std::string data = "01234567";
            chunk.resize(data.size());
            chunk.copy_from_bytes(data.data(), data.size());
            return false;
        });
        resp.set_status(HttpStatus::Ok);
        resp.set_content_type("text/plain");
    });

    scope.start();

    RawHttpClient client;
    ASSERT_TRUE(client.connect());
    ASSERT_TRUE(client.send("GET /api/short HTTP/1.1\r\nHost: localhost\r\n\r\n"));

    auto response = client.read_response();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status(), HttpStatus::Ok);
    EXPECT_EQ(response->http_header().content_length(), 64u);
    EXPECT_EQ(response->content().size(), 8u);
    // the announced length is not reached: the connection is cut instead of being left open
    EXPECT_TRUE(client.wait_closed());
}

TEST_F(TestHttpServer, test_custom_404_page)
{
    ServerScope scope;
    TmpDir tmp;
    ASSERT_TRUE(tmp);

    const std::string page = fs::combine(tmp.path(), "404.html");
    ASSERT_TRUE(fs::write(page, "<html>custom not found</html>"));
    scope.server.set_404_path(page);

    int hits = 0;
    scope.server._webservice->set_entry_point(
        "known",
        [&hits](const HttpRequest &, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    auto missing = sihd::http::get("localhost:3001/api/unknown");
    ASSERT_TRUE(missing.has_value());
    EXPECT_EQ(missing->status(), HttpStatus::NotFound);
    EXPECT_EQ(missing->content().cpp_str(), "<html>custom not found</html>");

    // the same page answers a body carrying request to an unknown route
    auto posted = sihd::http::post("localhost:3001/api/unknown", "body");
    ASSERT_TRUE(posted.has_value());
    EXPECT_EQ(posted->status(), HttpStatus::NotFound);
    EXPECT_EQ(posted->content().cpp_str(), "<html>custom not found</html>");
    EXPECT_EQ(hits, 0);
}

TEST_F(TestHttpServer, test_streaming_response_to_body_request)
{
    ServerScope scope;

    scope.server._webservice->set_entry_point(
        "stream",
        [](const HttpRequest & req, HttpResponse & resp) {
            const std::string data = "streamed-" + req.content().cpp_str();
            resp.http_header().set_content_length(data.size());
            resp.set_stream_provider([data, sent = false](ArrByte & chunk) mutable -> bool {
                if (sent)
                    return false;
                chunk.resize(data.size());
                chunk.copy_from_bytes(data.data(), data.size());
                sent = true;
                return true;
            });
            resp.set_status(HttpStatus::Ok);
            resp.set_content_type("text/plain");
        },
        HttpRequest::Post);

    scope.start();

    auto resp = sihd::http::post("localhost:3001/api/stream", "payload");
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->content().cpp_str(), "streamed-payload");
}

TEST_F(TestHttpServer, test_body_longer_than_announced)
{
    ServerScope scope;

    int hits = 0;
    scope.server._webservice->set_entry_point(
        "strict",
        [&hits](const HttpRequest &, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    // 8 bytes of body behind a Content-Length of 4: the server cuts at the
    // announced length and answers 413 instead of serving the request
    const std::string request = "POST /api/strict HTTP/1.1\r\n"
                                "Host: localhost\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: 4\r\n"
                                "Connection: close\r\n"
                                "\r\n"
                                "01234567";

    RawHttpClient client;
    ASSERT_TRUE(client.connect());
    ASSERT_TRUE(client.send(request));

    auto response = client.read_response();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status(), HttpStatus::PayloadTooLarge);
    EXPECT_EQ(hits, 0);
}

TEST_F(TestHttpServer, test_cors_preflight)
{
    ServerScope scope;

    scope.server.set_cors_origin("https://example.com");
    scope.server._webservice->set_entry_point("data", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
    });

    scope.start();

    // OPTIONS preflight
    RequestOptions opts;
    opts.headers["Origin"] = "https://example.com";
    opts.headers["Access-Control-Request-Method"] = "POST";
    auto resp = sihd::http::options("localhost:3001/api/data", opts);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::NoContent);

    // a normal request answers with the CORS header too
    resp = sihd::http::get("localhost:3001/api/data", opts);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(resp->http_header().find("access-control-allow-origin"), "https://example.com");
}

TEST_F(TestHttpServer, test_per_webservice_auth)
{
    FeatureHttpServer server;
    // add a second webservice (public, no auth)
    auto *public_ws = server.add_child<WebService>("public");
    public_ws->set_entry_point("hello",
                               [](const HttpRequest &, HttpResponse & resp) { resp.set_plain_content("public-ok"); });

    // the "api" webservice gets its own authenticator
    TestAuth auth;
    server._webservice->set_authenticator(&auth);
    server._webservice->set_entry_point("secure", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("private-ok");
    });

    Worker w([&server] {
        server.start();
        return true;
    });
    server.set_port(3001);
    ASSERT_TRUE(w.start_sync_worker("test-server"));
    ASSERT_TRUE(server.wait_ready(std::chrono::milliseconds(100)));

    RequestOptions no_proxy;
    no_proxy.proxy = "";
    // public webservice accessible without credentials
    auto pub_resp = sihd::http::get("localhost:3001/public/hello", no_proxy);
    ASSERT_TRUE(pub_resp.has_value());
    EXPECT_EQ(pub_resp->status(), HttpStatus::Ok);
    EXPECT_EQ(pub_resp->content().cpp_str(), "public-ok");

    // protected webservice rejects without credentials
    auto priv_resp = sihd::http::get("localhost:3001/api/secure", no_proxy);
    ASSERT_TRUE(priv_resp.has_value());
    EXPECT_EQ(priv_resp->status(), HttpStatus::Unauthorized);

    // protected webservice accepts with correct token
    RequestOptions good_token;
    good_token.proxy = "";
    good_token.token = "my-secret-token-123";
    priv_resp = sihd::http::get("localhost:3001/api/secure", good_token);
    ASSERT_TRUE(priv_resp.has_value());
    EXPECT_EQ(priv_resp->status(), HttpStatus::Ok);
    EXPECT_EQ(priv_resp->content().cpp_str(), "private-ok");

    server.set_service_wait_stop(true);
    server.stop();
}

TEST_F(TestHttpServer, test_auth_context_propagation)
{
    ServerScope scope;
    TestAuth auth;

    std::string captured_user;
    std::string captured_token;

    scope.server._webservice->set_authenticator(&auth);
    scope.server._webservice->set_entry_point("whoami", [&](const HttpRequest & req, HttpResponse & resp) {
        captured_user = req.auth_user();
        captured_token = req.auth_token();
        resp.set_plain_content("ok");
    });

    scope.start();

    // basic auth → auth_user populated
    RequestOptions basic_creds;
    basic_creds.username = "admin";
    basic_creds.password = "secret";
    basic_creds.proxy = "";
    auto resp = sihd::http::get("localhost:3001/api/whoami", basic_creds);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(captured_user, "admin");
    EXPECT_TRUE(captured_token.empty());

    // token auth → auth_token populated
    captured_user.clear();
    captured_token.clear();
    RequestOptions token_opts;
    token_opts.token = "my-secret-token-123";
    token_opts.proxy = "";
    resp = sihd::http::get("localhost:3001/api/whoami", token_opts);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_TRUE(captured_user.empty());
    EXPECT_EQ(captured_token, "my-secret-token-123");
}

TEST_F(TestHttpServer, test_concurrent_service)
{
    if constexpr (sihd::util::build::is_run_with_tsan)
        GTEST_SKIP_("has weird interaction with lws and tsan");

    constexpr bool run_monothreaded = false;
    constexpr bool run_multithreaded = true;
    constexpr int num_threads = 4;
    constexpr int num_requests = 10000;
    constexpr int batch_size = 50; // stay well below ulimit -n (default 1024)
    constexpr int port = 3002;
    const std::string url = "localhost:" + std::to_string(port) + "/api/task";
    Stopwatch stopwatch;

    auto filter = std::make_unique<sihd::util::LoggerFilter>(
        sihd::util::LoggerFilter::Options {.level_lower = sihd::util::LogLevel::warning});
    sihd::util::TmpLoggerFilterAdder filter_adder(filter.get());

    auto run_batched = [&](int handler_calls_expected, auto & handler_calls) -> sihd::util::time::UnixTime {
        stopwatch.reset();
        int completed = 0;
        while (completed < num_requests)
        {
            int batch = std::min(batch_size, num_requests - completed);
            std::vector<FutureHttpResponse> futures;
            futures.reserve(batch);
            for (int i = 0; i < batch; ++i)
                futures.push_back(sihd::http::async_get(url));
            for (int i = 0; i < batch; ++i)
            {
                auto resp = futures[i].get();
                EXPECT_TRUE(resp.has_value());
                if (resp.has_value())
                {
                    EXPECT_EQ(resp->status(), HttpStatus::Ok);
                    EXPECT_EQ(resp->content().cpp_str(), "pool-ok");
                }
            }
            completed += batch;
        }
        EXPECT_EQ(handler_calls.load(), handler_calls_expected);
        return sihd::util::time::to_milli(stopwatch.time());
    };

    // --- Baseline: single service thread ---
    if constexpr (run_monothreaded)
    {
        ServerScope scope;
        std::atomic<int> handler_calls {0};
        scope.server._webservice->set_entry_point("task", [&](const HttpRequest &, HttpResponse & resp) {
            ++handler_calls;
            resp.set_plain_content("pool-ok");
        });
        scope.start(port);

        auto no_pool_ms = run_batched(num_requests, handler_calls);
        SIHD_LOG(warning, "BENCHMARK single-thread: {} requests in {}ms", num_requests, no_pool_ms);
    }

    // --- Multiple lws service threads ---
    if constexpr (run_multithreaded)
    {
        ServerScope scope;
        std::atomic<int> handler_calls {0};
        scope.server._webservice->set_entry_point("task", [&](const HttpRequest &, HttpResponse & resp) {
            ++handler_calls;
            resp.set_plain_content("pool-ok");
            resp.set_status(HttpStatus::Ok);
        });
        scope.server.set_service_thread_count(num_threads);
        scope.start(port);

        auto pool_ms = run_batched(num_requests, handler_calls);
        SIHD_LOG(warning, "BENCHMARK multi-thread: {} requests in {}ms", num_requests, pool_ms);
    }
}

TEST_F(TestHttpServer, test_negative_content_length)
{
    ServerScope scope;

    int hits = 0;
    scope.server._webservice->set_entry_point(
        "echo",
        [&hits](const HttpRequest & req, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content(req.content().cpp_str());
        },
        HttpRequest::Post);

    scope.start();

    // a signed length is an invalid request, not a huge allocation
    {
        RawHttpClient client;
        ASSERT_TRUE(client.connect());
        ASSERT_TRUE(client.send("POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n"));
        auto negative = client.read_response();
        ASSERT_TRUE(negative.has_value());
        EXPECT_EQ(negative->status(), HttpStatus::BadRequest);
        EXPECT_EQ(hits, 0);
        EXPECT_TRUE(client.wait_closed());
    }

    // fresh connection: a decorated length is invalid too
    RawHttpClient client;
    ASSERT_TRUE(client.connect());
    ASSERT_TRUE(client.send("POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0x10\r\n\r\n"));
    auto hexa = client.read_response();
    ASSERT_TRUE(hexa.has_value());
    EXPECT_EQ(hexa->status(), HttpStatus::BadRequest);
    EXPECT_EQ(hits, 0);
    EXPECT_TRUE(client.wait_closed());
}

TEST_F(TestHttpServer, test_negative_content_length_on_reused_connection)
{
    ServerScope scope;

    int hits = 0;
    scope.server._webservice->set_entry_point(
        "echo",
        [&hits](const HttpRequest & req, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content(req.content().cpp_str());
        },
        HttpRequest::Post);

    scope.start();

    RawHttpClient client;
    ASSERT_TRUE(client.connect());

    // a served body leaves its buffer capacity in the session: the keep-alive
    // request below must not spin the allocator
    ASSERT_TRUE(client.send("POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello"));
    auto served = client.read_response();
    ASSERT_TRUE(served.has_value());
    ASSERT_EQ(served->status(), HttpStatus::Ok);
    ASSERT_EQ(hits, 1);

    ASSERT_TRUE(client.send("POST /api/echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n"));
    auto negative = client.read_response();
    ASSERT_TRUE(negative.has_value());
    EXPECT_EQ(negative->status(), HttpStatus::BadRequest);
    EXPECT_EQ(hits, 1);
    EXPECT_TRUE(client.wait_closed());
}

TEST_F(TestHttpServer, test_buffered_body_over_limit)
{
    ServerScope scope;

    int hits = 0;
    scope.server._webservice->set_entry_point(
        "big",
        [&hits](const HttpRequest &, HttpResponse & resp) {
            ++hits;
            resp.set_plain_content("ok");
        },
        HttpRequest::Post);

    scope.start();

    RawHttpClient client;
    ASSERT_TRUE(client.connect());

    // a buffered body over the memory limit is refused without reading it
    ASSERT_TRUE(client.send("POST /api/big HTTP/1.1\r\nHost: localhost\r\nContent-Length: 99999999\r\n\r\n"));
    auto refused = client.read_response();
    ASSERT_TRUE(refused.has_value());
    EXPECT_EQ(refused->status(), HttpStatus::PayloadTooLarge);
    EXPECT_EQ(hits, 0);
    EXPECT_TRUE(client.wait_closed());
}

} // namespace test
