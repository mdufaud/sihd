#include <gtest/gtest.h>

#include <sihd/sys/network.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

class TestNetwork: public ::testing::Test
{
    protected:
        TestNetwork() { LoggerManager::stream(); }

        virtual ~TestNetwork() { LoggerManager::clear_loggers(); }
};

TEST_F(TestNetwork, test_connectivity_is_readable)
{
    auto connectivity = network::connectivity();

    ASSERT_TRUE(connectivity.has_value()) << connectivity.error().message;
    EXPECT_NE(*connectivity, network::Connectivity::portal);
}

} // namespace test
