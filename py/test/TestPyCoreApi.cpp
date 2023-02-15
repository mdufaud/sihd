#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>
#include <sihd/py/PyCoreApi.hpp>
#include <pybind11/embed.h>

#include "DirectorySwitcher.hpp"

namespace test
{
    SIHD_LOGGER;
    using namespace sihd::py;
    class TestPyCoreApi:   public ::testing::Test
    {
        protected:
            TestPyCoreApi()
            {
                sihd::util::LoggerManager::basic();
            }

            virtual ~TestPyCoreApi()
            {
                sihd::util::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }

            std::string _cwd;
            std::string _base_test_dir;
    };

    TEST_F(TestPyCoreApi, test_pycore_channel)
    {
        DirectorySwitcher d(getenv("LIB_PATH"));
        pybind11::scoped_interpreter guard{};
        EXPECT_NO_THROW(pybind11::eval_file(_cwd + "/test/py/core/test_channel.py"));
    }

    TEST_F(TestPyCoreApi, test_pycore_devpulsation)
    {
        DirectorySwitcher d(getenv("LIB_PATH"));
        pybind11::scoped_interpreter guard{};
        EXPECT_NO_THROW(pybind11::eval_file(_cwd + "/test/py/core/test_devpulsation.py"));
    }
}
