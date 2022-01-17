#include <gtest/gtest.h>
#include <iostream>
#include <sihd/util/Logger.hpp>
#include <sihd/py/PyUtilApi.hpp>
#include <pybind11/embed.h>

namespace test
{
    NEW_LOGGER("test");
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
        {
            TRACE("Changing directory to: " << libpath);
            chdir(libpath.c_str());
        }
        pybind11::scoped_interpreter guard{};
        EXPECT_NO_THROW(pybind11::exec(R"(
            import sihd_util
            root = sihd_util.Node("root")
            parent1 = sihd_util.Node("parent1", root)
            parent2 = sihd_util.Node("parent2", root)
            sub_parent1 = sihd_util.Node("sub_parent", parent1)
            sub_parent2 = sihd_util.Node("sub_parent", parent2)
            named1 = sihd_util.Named("child", sub_parent1)
            named2 = sihd_util.Named("child", sub_parent2)
            named_parentless = sihd_util.Named("orphan")
            print(root.get_name())
            print(named1.get_name())
        )"));
    }
}
