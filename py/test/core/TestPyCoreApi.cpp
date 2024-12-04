#include <iostream>

#include <gtest/gtest.h>
#include <pybind11/embed.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/fs.hpp>
#include <sihd/util/os.hpp>
#include <sihd/util/term.hpp>

#include <sihd/py/core/PyCoreApi.hpp>

#include "../DirectorySwitcher.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::py;
class TestPyCoreApi: public ::testing::Test
{
    protected:
        TestPyCoreApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPyCoreApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPyCoreApi, test_pycore_channel)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/core/py/test_channel.py"));
}

TEST_F(TestPyCoreApi, test_pycore_devpulsation)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/core/py/test_devpulsation.py"));
}
} // namespace test
