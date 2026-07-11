#include <pybind11/embed.h>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>

#include <sihd/py/net/PyNetApi.hpp>

#include "../DirectorySwitcher.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::py;
class TestPyNetApi: public ::testing::Test
{
    protected:
        TestPyNetApi() { sihd::util::LoggerManager::stream(); }

        virtual ~TestPyNetApi() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestPyNetApi, test_pynet_base)
{
    DirectorySwitcher d(getenv("LIB_PATH"));
    pybind11::scoped_interpreter guard {};
    EXPECT_NO_THROW(pybind11::eval_file(d.old_cwd() + "/test/net/py/test_net.py"));
}
} // namespace test
