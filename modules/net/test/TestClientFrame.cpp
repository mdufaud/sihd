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

    auto decoded = ClientFrame::decode(frame);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->client_id(), 42u);
    EXPECT_EQ(decoded->payload().str(), "hello");
}

TEST_F(TestClientFrame, test_clientframe_empty_payload)
{
    const ArrByte frame = ClientFrame(7, "").encode();

    auto decoded = ClientFrame::decode(frame);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->client_id(), 7u);
    EXPECT_EQ(decoded->payload().size(), 0u);
}

TEST_F(TestClientFrame, test_clientframe_shorter_than_header)
{
    const ArrByte frame = {1, 2, 3};

    auto decoded = ClientFrame::decode(frame);
    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error().code, ErrorCode::invalid_argument);
}

} // namespace test
