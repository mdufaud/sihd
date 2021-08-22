#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/py/PyUtilApi.hpp>
#include <pybind11/embed.h>

namespace test
{
    LOGGER;
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

    TEST_F(TestPyUtilApi, test_py_util_api)
    {
        std::string libpath = getenv("LIB_PATH");
        if (libpath.empty() == false)
            chdir(libpath.c_str());
        pybind11::scoped_interpreter guard{};
        //EXPECT_NO_THROW(pybind11::module_::import("sihd_py"));
        pybind11::exec(R"(
            import sihd_py
            root = sihd_py.Node("root", None)
            named = sihd_py.Named("child", None)
            print(root.name())
            print(named.name())
            print('hello world')
        )");
    }
}
