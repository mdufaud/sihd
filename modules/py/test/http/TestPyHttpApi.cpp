#include <pybind11/embed.h>

#include <http_test_helpers.hpp>

#include <gtest/gtest.h>

#include <sihd/py/http/PyHttpApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../DirectorySwitcher.hpp"
#include "../PyInterpreter.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::py;
using namespace sihd::http;
using namespace sihd::util;

class TestPyHttpApi: public ::testing::Test
{
    protected:
        TestPyHttpApi() { LoggerManager::stream(); }

        virtual ~TestPyHttpApi() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _server = std::make_unique<EchoServerScope>();
            _server->server.set_cors_origin("https://app.com");
            _server->start(3012);
        }

        virtual void TearDown() { _server.reset(); }

        std::unique_ptr<EchoServerScope> _server;
};

TEST_F(TestPyHttpApi, test_pyhttp_navigator)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/http/py/test_http.py"));
}

TEST_F(TestPyHttpApi, test_pyhttp_server)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/http/py/test_server.py"));
}
} // namespace test
