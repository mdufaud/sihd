#include <pybind11/embed.h>

#include <iostream>

#include <gtest/gtest.h>

#include <sihd/py/util/PyUtilApi.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>

#include "../DirectorySwitcher.hpp"
#include "../PyInterpreter.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::py;
class TestPyUtilApi: public ::testing::Test
{
    protected:
        TestPyUtilApi() { LoggerManager::stream(); }

        virtual ~TestPyUtilApi() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPyUtilApi, test_pyutil_node)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/util/py/test_node.py"));
}

TEST_F(TestPyUtilApi, test_pyutil_log)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/util/py/test_log.py"));
}

TEST_F(TestPyUtilApi, test_pyutil_thread)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/util/py/test_thread.py"));
}

TEST_F(TestPyUtilApi, test_pyutil_array)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/util/py/test_array.py"));
}
} // namespace test
