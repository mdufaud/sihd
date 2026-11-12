#include <gtest/gtest.h>

#include <sihd/bt/GattServer.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;

class TestBtGattServer: public ::testing::Test
{
    protected:
        TestBtGattServer(): _server("server") { LoggerManager::stream(); }

        virtual ~TestBtGattServer() { LoggerManager::clear_loggers(); }

        GattServer _server;
};

TEST_F(TestBtGattServer, test_notify_before_start)
{
    const std::vector<uint8_t> value {0x01};

    auto notified = _server.notify("0000180d-0000-1000-8000-00805f9b34fb",
                                   "00002a37-0000-1000-8000-00805f9b34fb",
                                   value);

    ASSERT_FALSE(notified.has_value());
    EXPECT_EQ(notified.error().code, ErrorCode::closed);
}

TEST_F(TestBtGattServer, test_secondary_service_refused)
{
    ASSERT_TRUE(_server.add_service(
        LocalService {.uuid = "0000180d-0000-1000-8000-00805f9b34fb", .primary = false, .characteristics = {}}));

    EXPECT_FALSE(_server.start());
}

} // namespace test
