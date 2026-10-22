#include <pybind11/embed.h>

#include <iostream>

#include <gtest/gtest.h>

#include <sihd/py/sys/PySysApi.hpp>
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
class TestPySysApi: public ::testing::Test
{
    protected:
        TestPySysApi() { LoggerManager::stream(); }

        virtual ~TestPySysApi() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPySysApi, test_pysys_fs)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/sys/py/test_fs.py"));
}

TEST_F(TestPySysApi, test_pysys_os)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/sys/py/test_os.py"));
}

TEST_F(TestPySysApi, test_pysys_user)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/sys/py/test_user.py"));
}

TEST_F(TestPySysApi, test_pysys_signal)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/sys/py/test_signal.py"));
}

TEST_F(TestPySysApi, test_pysys_uuid)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/sys/py/test_uuid.py"));
}

TEST_F(TestPySysApi, test_pysys_process_info)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/sys/py/test_process_info.py"));
}

TEST_F(TestPySysApi, test_pysys_bitmap)
{
    DirectorySwitcher dir_switcher(lib_path());
    require_py_environment();
    EXPECT_NO_THROW(pybind11::eval_file(dir_switcher.old_cwd() + "/test/sys/py/test_bitmap.py"));
}

} // namespace test
