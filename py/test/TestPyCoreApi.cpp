#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Files.hpp>
#include <sihd/util/OS.hpp>
#include <sihd/util/Term.hpp>
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
                char *test_path = getenv("TEST_PATH");
                _base_test_dir = sihd::util::Files::combine({
                    test_path == nullptr ? "unit_test" : test_path,
                    "py",
                    "pycoreapi"
                });
                _cwd = sihd::util::OS::get_cwd();
                sihd::util::LoggerManager::basic();
                sihd::util::Files::make_directories(_base_test_dir);
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

    TEST_F(TestPyCoreApi, test_py_core_api)
    {
        DirectorySwitcher d(getenv("LIB_PATH"));
        pybind11::scoped_interpreter guard{};
        EXPECT_NO_THROW(pybind11::eval_file(_cwd + "/test/py/core/test_channel.py"));
    }
}
