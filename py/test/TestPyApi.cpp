#include <gtest/gtest.h>
#include <iostream>
#include <sihd/core/Logger.hpp>
#include <sihd/py/PyApi.hpp>
#include <pybind11/embed.h>

namespace test
{
    NEW_LOGGER("test");

    using namespace sihd::py;
    using namespace pybind11::literals;

    class TestPyApi:   public ::testing::Test
    {
        protected:
            TestPyApi()
            {
                sihd::core::LoggerManager::basic();
            }

            virtual ~TestPyApi()
            {
                sihd::core::LoggerManager::clear_loggers();
            }

            virtual void SetUp()
            {
            }

            virtual void TearDown()
            {
            }
    };

    TEST_F(TestPyApi, test_py_interpreter)
    {
        Py_Initialize();
        Py_Finalize();
    }

    TEST_F(TestPyApi, test_pybind11_interpreter)
    {
        pybind11::scoped_interpreter guard{};
        pybind11::object scope = pybind11::dict("name"_a="World", "number"_a=43);
        pybind11::eval_file("py/test/py/test_import.py", scope);
    }
}
