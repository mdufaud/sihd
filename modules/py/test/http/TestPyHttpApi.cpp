#include <pybind11/embed.h>

#include <http_test_helpers.hpp>

#include <gtest/gtest.h>

#include <sihd/py/http/PyHttpApi.hpp>
#include <sihd/util/Logger.hpp>

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
        TestPyHttpApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPyHttpApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _server = std::make_unique<EchoServerScope>();
            _server->start(3012);
        }

        virtual void TearDown() { _server.reset(); }

        std::unique_ptr<EchoServerScope> _server;
};

TEST_F(TestPyHttpApi, test_pyhttp_navigator)
{
    DirectorySwitcher d(lib_path());
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/http/py/test_http.py"));
}

TEST_F(TestPyHttpApi, test_pyhttp_server)
{
    DirectorySwitcher d(lib_path());
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/http/py/test_server.py"));
}
} // namespace test
