#include <stdexcept>

#include <gtest/gtest.h>

#include <sihd/net/ClientFrame.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;

class TestClientFrame: public ::testing::Test
{
    protected:
        TestClientFrame() { LoggerManager::stream(); }

        virtual ~TestClientFrame() { LoggerManager::clear_loggers(); }
};

TEST_F(TestClientFrame, test_clientframe_round_trip)
{
    const ArrByte frame = ClientFrame(42, "hello").encode();

    EXPECT_EQ(frame.size(), 13u);

    const ClientFrame decoded(frame);
    EXPECT_EQ(decoded.client_id(), 42u);
    EXPECT_EQ(decoded.payload().str(), "hello");
}

TEST_F(TestClientFrame, test_clientframe_empty_payload)
{
    const ArrByte frame = ClientFrame(7, "").encode();

    ClientFrame decoded;
    ASSERT_TRUE(decoded.decode(frame).has_value());
    EXPECT_EQ(decoded.client_id(), 7u);
    EXPECT_EQ(decoded.payload().size(), 0u);
}

TEST_F(TestClientFrame, test_clientframe_shorter_than_header)
{
    const ArrByte frame = {1, 2, 3};
    ClientFrame decoded(9, "kept");

    auto refused = decoded.decode(frame);

    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(refused.error().message, "client frame shorter than its header");
    EXPECT_EQ(decoded.client_id(), 9u);
    EXPECT_EQ(decoded.payload().str(), "kept");
    EXPECT_THROW({ ClientFrame thrown(frame); }, std::runtime_error);
}

} // namespace test
