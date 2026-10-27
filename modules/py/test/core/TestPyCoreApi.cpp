#include <pybind11/embed.h>

#include <iostream>

#include <gtest/gtest.h>

#include <sihd/py/core/PyCoreApi.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/sys/platform.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/ansi.hpp>

#include "../DirectorySwitcher.hpp"
#include "../PyInterpreter.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::py;
class TestPyCoreApi: public ::testing::Test
{
    protected:
        TestPyCoreApi() { LoggerManager::stream(); }

        virtual ~TestPyCoreApi() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPyCoreApi, test_pycore_channel)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/core/py/test_channel.py"));
}

TEST_F(TestPyCoreApi, test_pycore_devpulsation)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/core/py/test_devpulsation.py"));
}

TEST_F(TestPyCoreApi, test_pycore_devworker)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/core/py/test_devworker.py"));
}

TEST_F(TestPyCoreApi, test_pycore_devworkerpool)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/core/py/test_devworkerpool.py"));
}

TEST_F(TestPyCoreApi, test_pycore_devbatcher)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/core/py/test_devbatcher.py"));
}

TEST_F(TestPyCoreApi, test_pycore_devstats)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/core/py/test_devstats.py"));
}

TEST_F(TestPyCoreApi, test_pycore_observer)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/core/py/test_observer.py"));
}

TEST_F(TestPyCoreApi, test_pycore_observer_threads)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/core/py/test_observer_threads.py"));
}
} // namespace test
