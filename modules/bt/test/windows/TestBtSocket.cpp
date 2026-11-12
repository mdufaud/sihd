#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include <sihd/bt/Socket.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;

class TestBtSocket: public ::testing::Test
{
    protected:
        TestBtSocket() { LoggerManager::stream(); }

        virtual ~TestBtSocket() { LoggerManager::clear_loggers(); }
};

TEST_F(TestBtSocket, test_l2cap_not_supported)
{
    Socket socket;

    auto connected = socket.connect_l2cap("00:11:22:33:44:55", AddressType::public_address, 0x80);
    auto listening = socket.listen_l2cap(0);

    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error().code, ErrorCode::not_supported);
    ASSERT_FALSE(listening.has_value());
    EXPECT_EQ(listening.error().code, ErrorCode::not_supported);
    EXPECT_THROW({ Socket refused("00:11:22:33:44:55", AddressType::public_address, 0x80); }, std::runtime_error);
    EXPECT_THROW({ Socket listener(uint16_t {0}); }, std::runtime_error);
}

TEST_F(TestBtSocket, test_invalid_arguments)
{
    const std::string uuid = "00001101-0000-1000-8000-00805f9b34fb";
    Socket socket;

    auto bad_address = socket.connect_rfcomm("00:11:22:33:44", uuid);
    auto bad_uuid = socket.connect_rfcomm("00:11:22:33:44:55", "1101");
    auto bad_listener = socket.listen_rfcomm("not-a-uuid", "service");

    EXPECT_FALSE(socket.is_open());
    ASSERT_FALSE(bad_address.has_value());
    EXPECT_EQ(bad_address.error().code, ErrorCode::invalid_argument);
    ASSERT_FALSE(bad_uuid.has_value());
    EXPECT_EQ(bad_uuid.error().code, ErrorCode::invalid_argument);
    ASSERT_FALSE(bad_listener.has_value());
    EXPECT_EQ(bad_listener.error().code, ErrorCode::invalid_argument);
}

} // namespace test
