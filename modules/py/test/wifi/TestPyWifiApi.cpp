#include <pybind11/embed.h>

#include <gtest/gtest.h>

#include <sihd/py/wifi/PyWifiApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../DirectorySwitcher.hpp"
#include "../PyInterpreter.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::py;
class TestPyWifiApi: public ::testing::Test
{
    protected:
        TestPyWifiApi() { LoggerManager::stream(); }

        virtual ~TestPyWifiApi() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPyWifiApi, test_pywifi_base)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/wifi/py/test_wifi.py"));
}
} // namespace test
