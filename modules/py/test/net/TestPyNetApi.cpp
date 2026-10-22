#include <pybind11/embed.h>

#include <gtest/gtest.h>

#include <sihd/py/net/PyNetApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../DirectorySwitcher.hpp"
#include "../PyInterpreter.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::py;
class TestPyNetApi: public ::testing::Test
{
    protected:
        TestPyNetApi() { LoggerManager::stream(); }

        virtual ~TestPyNetApi() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPyNetApi, test_pynet_base)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/net/py/test_net.py"));
}
} // namespace test
