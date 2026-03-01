#include <pybind11/embed.h>

#include <iostream>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

#include <sihd/py/sys/PySysApi.hpp>

#include "../DirectorySwitcher.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;
using namespace sihd::py;
class TestPySysApi: public ::testing::Test
{
    protected:
        TestPySysApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPySysApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPySysApi, test_pysys_fs)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/sys/py/test_fs.py"));
}

TEST_F(TestPySysApi, test_pysys_os)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/sys/py/test_os.py"));
}

TEST_F(TestPySysApi, test_pysys_signal)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/sys/py/test_signal.py"));
}

TEST_F(TestPySysApi, test_pysys_uuid)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/sys/py/test_uuid.py"));
}

TEST_F(TestPySysApi, test_pysys_process_info)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/sys/py/test_process_info.py"));
}
} // namespace test
