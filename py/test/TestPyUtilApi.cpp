#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/Term.hpp>
#include <sihd/py/PyUtilApi.hpp>
#include <pybind11/embed.h>

#include "DirectorySwitcher.hpp"

namespace test
{
    SIHD_NEW_LOGGER("test");
    using namespace sihd::util;
    using namespace sihd::py;
    class TestPyUtilApi:   public ::testing::Test
    {
        protected:
            TestPyUtilApi()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestPyUtilApi()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestPyUtilApi, test_pyutil_node)
    {
        DirectorySwitcher d(getenv("LIB_PATH"));
        pybind11::scoped_interpreter guard{};
        EXPECT_NO_THROW(pybind11::eval_file(_cwd + "/test/py/util/test_node.py"));
    }

    TEST_F(TestPyUtilApi, test_pyutil_thread)
    {
        DirectorySwitcher d(getenv("LIB_PATH"));
        pybind11::scoped_interpreter guard{};
        EXPECT_NO_THROW(pybind11::eval_file(_cwd + "/test/py/util/test_thread.py"));
    }

    TEST_F(TestPyUtilApi, test_pyutil_array)
    {
        DirectorySwitcher d(getenv("LIB_PATH"));
        pybind11::scoped_interpreter guard{};
        EXPECT_NO_THROW(pybind11::eval_file(_cwd + "/test/py/util/test_array.py"));
    }
}
