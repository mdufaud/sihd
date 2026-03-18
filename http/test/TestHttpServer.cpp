#include "sihd/util/LogInfo.hpp"
#include "sihd/util/LoggerFilter.hpp"
#include "sihd/util/LoggerManager.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/HttpStatus.hpp>
#include <sihd/http/IHttpAuthenticator.hpp>
#include <sihd/http/IHttpFilter.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/WebsocketHandler.hpp>
#include <sihd/http/request.hpp>
#include <sihd/sys/File.hpp>
#include <sihd/sys/SigWatcher.hpp>
#include <sihd/sys/TmpDir.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>
#include <sihd/util/time.hpp>
#include <string_view>

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

        void on_open(std::string_view protocol_name)
        {
            SIHD_LOG(debug, "Opened websocket of protocol: {}", protocol_name);
            ++_nopen;
        };

        bool on_read(const sihd::util::ArrChar & array)
        {
            SIHD_LOG(debug, "Read from client websocket: {}", array.str());
            _client_wrote = true;
            ++_nread;
            return true;
        };

        bool on_write(sihd::util::ArrChar & array, WriteProtocol & protocol)
        {
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

        void on_close()
        {
            SIHD_LOG(debug, "Closed websocket");
            ++_nclosed;
            this->request_stop();
        }

        void on_peer_close(uint16_t code, std::string_view reason)
        {
            SIHD_LOG(debug, "Peer closed websocket: code={} reason={}", code, reason);
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

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    TmpDir tmpdir;
    std::string tmpfile_path = fs::combine(tmpdir.path(), "test_file.txt");
    fs::write(tmpfile_path, "hello put world");

    // launch all requests async
    auto get_future = sihd::http::async_get("localhost:3000/web/some_get");
    auto post_future = sihd::http::async_post("localhost:3000/web/some_post", "hello world");
    auto put_future = sihd::http::async_put("localhost:3000/web/some_put", tmpfile_path);
    auto del_future = sihd::http::async_del("localhost:3000/web/some_delete");

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

    //
    EXPECT_GT(server._nget, 0);
    EXPECT_GT(server._npost, 0);
    EXPECT_GT(server._nput, 0);
    EXPECT_GT(server._ndelete, 0);

    EXPECT_GT(server._nopen, 0);
    EXPECT_GT(server._nread, 0);
    EXPECT_GT(server._nwrite, 0);
    EXPECT_GT(server._nclosed, 0);
}

// --- Test helpers for new features ---

class TestFilter: public sihd::http::IHttpFilter
{
    public:
        std::string blocked_uri;
        std::string last_uri;
        std::string last_client_ip;
        int filter_calls = 0;

        bool on_filter_connection(const HttpFilterInfo & info) override
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
            _webservice = this->add_child<WebService>("api");
        }

        ~FeatureHttpServer() = default;

        WebService *_webservice;
};

struct ServerScope
{
        FeatureHttpServer server;
        Worker worker;

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

TEST_F(TestHttpServer, test_routing_params)
{
    ServerScope scope;

    std::string captured_id;
    std::string captured_action;

    scope.server._webservice->set_entry_point("users/{id}",
                                              [&](const HttpRequest & req, HttpResponse & resp) {
                                                  auto id = req.path_param("id");
                                                  captured_id = id.value_or("");
                                                  resp.set_plain_content(fmt::format("user:{}", captured_id));
                                              });

    scope.server._webservice->set_entry_point(
        "users/{id}/{action}",
        [&](const HttpRequest & req, HttpResponse & resp) {
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

    scope.server._webservice->set_entry_point("files/{path...}",
                                              [&](const HttpRequest & req, HttpResponse & resp) {
                                                  captured_path
                                                      = std::string(req.path_param("path").value_or(""));
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
    CurlOptions bad_creds;
    bad_creds.username = "admin";
    bad_creds.password = "wrong";
    auto resp = sihd::http::get("localhost:3001/api/protected", bad_creds);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    // wrong user → 401
    CurlOptions bad_user;
    bad_user.username = "nobody";
    bad_user.password = "secret";
    resp = sihd::http::get("localhost:3001/api/protected", bad_user);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    // correct credentials → 200
    CurlOptions good_creds;
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
    CurlOptions bad_token;
    bad_token.token = "invalid-token";
    resp = sihd::http::get("localhost:3001/api/secure", bad_token);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Unauthorized);

    // correct token → 200
    CurlOptions good_token;
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
    CurlOptions basic_creds;
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

TEST_F(TestHttpServer, test_cors_preflight)
{
    ServerScope scope;

    scope.server.set_cors_origin("https://example.com");
    scope.server._webservice->set_entry_point("data", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("ok");
    });

    scope.start();

    // OPTIONS preflight
    CurlOptions opts;
    opts.headers["Origin"] = "https://example.com";
    opts.headers["Access-Control-Request-Method"] = "POST";
    auto resp = sihd::http::options("localhost:3001/api/data", opts);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::NoContent);

    // normal GET should also have CORS header
    resp = sihd::http::get("localhost:3001/api/data", opts);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
}

TEST_F(TestHttpServer, test_per_webservice_auth)
{
    FeatureHttpServer server;
    // add a second webservice (public, no auth)
    auto *public_ws = server.add_child<WebService>("public");
    public_ws->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
        resp.set_plain_content("public-ok");
    });

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
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // public webservice accessible without credentials
    auto pub_resp = sihd::http::get("localhost:3001/public/hello");
    ASSERT_TRUE(pub_resp.has_value());
    EXPECT_EQ(pub_resp->status(), HttpStatus::Ok);
    EXPECT_EQ(pub_resp->content().cpp_str(), "public-ok");

    // protected webservice rejects without credentials
    auto priv_resp = sihd::http::get("localhost:3001/api/secure");
    ASSERT_TRUE(priv_resp.has_value());
    EXPECT_EQ(priv_resp->status(), HttpStatus::Unauthorized);

    // protected webservice accepts with correct token
    CurlOptions good_token;
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
    CurlOptions basic_creds;
    basic_creds.username = "admin";
    basic_creds.password = "secret";
    auto resp = sihd::http::get("localhost:3001/api/whoami", basic_creds);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_EQ(captured_user, "admin");
    EXPECT_TRUE(captured_token.empty());

    // token auth → auth_token populated
    captured_user.clear();
    captured_token.clear();
    CurlOptions token_opts;
    token_opts.token = "my-secret-token-123";
    resp = sihd::http::get("localhost:3001/api/whoami", token_opts);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->status(), HttpStatus::Ok);
    EXPECT_TRUE(captured_user.empty());
    EXPECT_EQ(captured_token, "my-secret-token-123");
}

TEST_F(TestHttpServer, test_thread_pool)
{
    constexpr bool run_monothreaded = false;
    constexpr bool run_multithreaded = true;
    constexpr int num_threads = 8;
    constexpr int num_requests = 10000;
    constexpr int batch_size = 50; // stay well below ulimit -n (default 1024)
    constexpr int port = 3002;
    const std::string url = "localhost:" + std::to_string(port) + "/api/task";
    Stopwatch stopwatch;

    sihd::util::TmpLoggerFilterAdder filter_adder(
        new sihd::util::LoggerFilter({.level_lower = sihd::util::LogLevel::warning}));

    auto run_batched = [&](int handler_calls_expected, auto & handler_calls) -> time_t {
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

    // --- Baseline: no thread pool ---
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
        SIHD_LOG(warning, "BENCHMARK no-pool: {} requests in {}ms", num_requests, no_pool_ms);
    }

    // --- Thread pool ---
    if constexpr (run_multithreaded)
    {
        ServerScope scope;
        std::atomic<int> handler_calls {0};
        scope.server._webservice->set_entry_point("task", [&](const HttpRequest &, HttpResponse & resp) {
            ++handler_calls;
            resp.set_plain_content("hello world");
            resp.set_status(HttpStatus::Ok);
        });
        scope.server.set_thread_pool_size(num_threads);
        scope.start(port);

        auto pool_ms = run_batched(num_requests, handler_calls);
        SIHD_LOG(warning, "BENCHMARK thread-pool: {} requests in {}ms", num_requests, pool_ms);
    }
}

} // namespace test
