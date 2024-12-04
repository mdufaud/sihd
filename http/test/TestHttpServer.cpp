#include <gtest/gtest.h>
#include <libwebsockets.h>
#include <nlohmann/json.hpp>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/http/WebsocketHandler.hpp>
#include <sihd/http/request.hpp>
#include <sihd/util/File.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/SigWatcher.hpp>
#include <sihd/util/TmpDir.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/str.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::http;
using namespace sihd::util;
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

        ~SimpleHttpServer() {}

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
                        resp.set_status(HTTP_STATUS_OK);
                        ++_npost;
                    }
                    else
                        resp.set_status(HTTP_STATUS_BAD_REQUEST);
                },
                HttpRequest::Post);

            _webservice->set_entry_point(
                "some_delete",
                [this](const HttpRequest & req, HttpResponse & resp) {
                    SIHD_LOG(info, "{} request received", req.type_str());
                    resp.set_status(HTTP_STATUS_OK);
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
                        _post_content = req.content().str();
                        SIHD_LOG(info, "Received PUT body: {}", _post_content);
                        resp.set_status(HTTP_STATUS_OK);
                        ++_nput;
                    }
                    else
                        resp.set_status(HTTP_STATUS_BAD_REQUEST);
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

        bool on_write(sihd::util::ArrChar & array, LwsWriteProtocol & protocol)
        {
            if (_client_wrote)
            {
                ++_nwrite;
                _client_wrote = false;

                const char hw[] = "hello world";
                array.from(hw);

                protocol.set_txt();
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

    const CurlOptions options = {
        .verbose = true,
        .ssl_verify_peer = false,
        .ssl_verify_host = false,
        .upload_progress =
            [&](long dltotal, long dlnow, long ultotal, long ulnow) {
                if (dltotal)
                    fmt::print("download: {}/{} bytes\n", dlnow, dltotal);
                if (ultotal)
                    fmt::print("upload: {}/{} bytes\n", ulnow, ultotal);
                return true;
            },
    };

    SIHD_LOG(info, "GET request");
    auto get_resp_opt = sihd::http::get("localhost:3000/web/some_get", options);
    if (get_resp_opt)
        fmt::print("{}", get_resp_opt->content().cpp_str());

    SIHD_LOG(info, "POST request");
    auto post_resp_opt = sihd::http::post("localhost:3000/web/some_post", "hello world", options);
    if (post_resp_opt)
        fmt::print("{}", post_resp_opt->content().cpp_str());

    // SIHD_LOG(info, "PUT request");
    // TmpDir tmpdir;
    // std::string tmpfile_path = fs::combine(tmpdir.path(), "test_file.txt");
    // fs::write(tmpfile_path, "hello world");
    // auto put_resp_opt = sihd::http::put("localhost:3000/web/some_put", tmpfile_path, options);
    // if (put_resp_opt)
    //     fmt::print("{}", put_resp_opt->content().cpp_str());

    SIHD_LOG(info, "DELETE request");
    auto delete_resp_opt = sihd::http::del("localhost:3000/web/some_delete", options);
    if (delete_resp_opt)
        fmt::print("{}", delete_resp_opt->content().cpp_str());

    server.set_service_wait_stop(true);
    server.stop();

    EXPECT_EQ(server._nget, 1);
    EXPECT_EQ(server._npost, 1);
    EXPECT_EQ(server._post_content, "hello world");
    // EXPECT_EQ(server._nput, 1);
    EXPECT_EQ(server._ndelete, 1);
} // namespace test

TEST_F(TestHttpServer, test_httpserver_websockets)
{
    if (sihd::util::term::is_interactive() == false)
        GTEST_SKIP_("requires interaction");

    SimpleHttpServer server;
    server.set_root_dir("test/resources/mount_point");
    server.set_port(3000);

    sihd::util::SigWatcher watcher("signal-watcher");

    watcher.add_signal(SIGINT);
    watcher.set_polling_frequency(5);

    Handler<SigWatcher *> sig_handler([&server](SigWatcher *watcher) {
        auto & catched_signals = watcher->catched_signals();
        if (catched_signals.empty())
            return;
        SIHD_LOG(info, "Stopping http server...");
        server.stop();
        SIHD_LOG(info, "Stopped http server");
    });
    watcher.add_observer(&sig_handler);
    watcher.start();

    SIHD_LOG(info, "=========================================================");
    SIHD_LOG(info, "Open web browser at localhost:3000 then close it");
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

} // namespace test
