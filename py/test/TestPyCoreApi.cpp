#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/py/PyCoreApi.hpp>
#include <pybind11/embed.h>

namespace test
{
    LOGGER;
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
    };

    TEST_F(TestPyCoreApi, test_py_core_api)
    {
        std::string libpath = getenv("LIB_PATH");
        if (libpath.empty() == false)
            chdir(libpath.c_str());
        pybind11::scoped_interpreter guard{};
        EXPECT_NO_THROW(pybind11::exec(R"(
            import sihd_core
            channel = sihd_core.Channel("chan", "int", 8)
            print(channel.get_name())
            import sihd_util
            root = sihd_util.Node("root", None)
            named = sihd_util.Named("child", None)
            print(root.get_name())
            print(named.get_name())
        )"));
    }
}
