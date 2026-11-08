#include <pybind11/embed.h>

#include <gtest/gtest.h>

#include <sihd/py/zlib/PyZlibApi.hpp>
#include <sihd/util/Logger.hpp>

#include "../DirectorySwitcher.hpp"
#include "../PyInterpreter.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::py;
class TestPyZlibApi: public ::testing::Test
{
    protected:
        TestPyZlibApi() { LoggerManager::stream(); }

        virtual ~TestPyZlibApi() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPyZlibApi, test_pyzlib_base)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/zlib/py/test_zlib.py"));
}
} // namespace test
