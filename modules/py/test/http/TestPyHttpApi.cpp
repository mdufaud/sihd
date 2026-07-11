#include <pybind11/embed.h>

#include <chrono>

#include <gtest/gtest.h>

#include <sihd/http/HttpServer.hpp>
#include <sihd/http/WebService.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Worker.hpp>

#include <sihd/py/http/PyHttpApi.hpp>

#include "../DirectorySwitcher.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::py;
using namespace sihd::http;
using namespace sihd::util;

class TestPyHttpApi: public ::testing::Test
{
    protected:
        TestPyHttpApi(): _worker([this] {
            _server->start();
            return true;
        })
        {
            sihd::util::LoggerManager::stream();
        }

        virtual ~TestPyHttpApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _server = std::make_unique<HttpServer>("http-server");
            WebService *webservice = _server->add_child<WebService>("api");
            webservice->set_entry_point("hello", [](const HttpRequest &, HttpResponse & resp) {
                resp.set_plain_content("navigator-ok");
            });
            webservice->set_entry_point(
                "echo",
                [](const HttpRequest & req, HttpResponse & resp) { resp.set_plain_content(req.content().cpp_str()); },
                HttpRequest::Post);

            ASSERT_TRUE(_server->set_port(3012));
            ASSERT_TRUE(_worker.start_sync_worker("test-http-server"));
            ASSERT_TRUE(_server->wait_ready(std::chrono::milliseconds(500)));
        }

        virtual void TearDown()
        {
            _server->set_service_wait_stop(true);
            _server->stop();
            _worker.stop_worker();
            _server.reset();
        }

        std::unique_ptr<HttpServer> _server;
        Worker _worker;
};

TEST_F(TestPyHttpApi, test_pyhttp_navigator)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/http/py/test_http.py"));
}

TEST_F(TestPyHttpApi, test_pyhttp_server)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/http/py/test_server.py"));
}
} // namespace test
