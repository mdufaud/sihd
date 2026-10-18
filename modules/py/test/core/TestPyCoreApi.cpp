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
using namespace sihd::py;
class TestPyCoreApi: public ::testing::Test
{
    protected:
        TestPyCoreApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPyCoreApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPyCoreApi, test_pycore_channel)
{
    DirectorySwitcher d(lib_path());
    py_interpreter();
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/core/py/test_channel.py"));
}

TEST_F(TestPyCoreApi, test_pycore_devpulsation)
{
    DirectorySwitcher d(lib_path());
    py_interpreter();
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/core/py/test_devpulsation.py"));
}

TEST_F(TestPyCoreApi, test_pycore_observer)
{
    DirectorySwitcher d(lib_path());
    py_interpreter();
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/core/py/test_observer.py"));
}

TEST_F(TestPyCoreApi, test_pycore_observer_threads)
{
    DirectorySwitcher d(lib_path());
    py_interpreter();
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/core/py/test_observer_threads.py"));
}
} // namespace test
