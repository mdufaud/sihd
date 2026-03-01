#include <pybind11/embed.h>

#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

#include <sihd/py/util/PyUtilApi.hpp>

#include "../DirectorySwitcher.hpp"

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::util;
using namespace sihd::py;
class TestPyUtilApi: public ::testing::Test
{
    protected:
        TestPyUtilApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPyUtilApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPyUtilApi, test_pyutil_node)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/util/py/test_node.py"));
}

TEST_F(TestPyUtilApi, test_pyutil_thread)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/util/py/test_thread.py"));
}

TEST_F(TestPyUtilApi, test_pyutil_array)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/util/py/test_array.py"));
}
} // namespace test
