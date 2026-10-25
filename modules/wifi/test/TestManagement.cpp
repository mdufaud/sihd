#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/wifi/ies.hpp>
#include <sihd/wifi/management.hpp>

#include "wifi_test_helpers.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;

class TestManagement: public ::testing::Test
{
    protected:
        TestManagement() { LoggerManager::stream(); }

        virtual ~TestManagement() { LoggerManager::clear_loggers(); }
};

TEST_F(TestManagement, test_management_beacon)
{
    const std::vector<uint8_t> body = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x64, 0x00, 0x01,
                                       0x04, 0x00, 0x04, 'h',  'o',  'm',  'e',  0x03, 0x01, 0x0b};

    const Result<Beacon> beacon = parse_beacon(ArrUByteView(body));

    ASSERT_TRUE(beacon.has_value());
    EXPECT_EQ(beacon->timestamp_us, 0x0102030405060708u);
    EXPECT_EQ(beacon->interval_tu, 100u);
    EXPECT_EQ(beacon->capability, 0x0401u);
    EXPECT_TRUE(beacon->ess);
    EXPECT_FALSE(beacon->ibss);
    EXPECT_FALSE(beacon->privacy);
    const Result<std::vector<ies::Element>> elements = ies::elements(beacon->information_elements);
    ASSERT_TRUE(elements.has_value());
    EXPECT_EQ(ies::ssid(*elements), "home");
    EXPECT_EQ(ies::channel(*elements), 11);
}

TEST_F(TestManagement, test_management_beacon_privacy_and_probe_response)
{
    const std::vector<uint8_t> body = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x13, 0x00};

    const Result<Beacon> beacon = parse_beacon(ArrUByteView(body));
    const Result<Beacon> response = parse_probe_response(ArrUByteView(body));

    ASSERT_TRUE(beacon.has_value());
    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(beacon->privacy);
    EXPECT_TRUE(beacon->ibss);
    EXPECT_EQ(response->capability, 0x0013u);
    EXPECT_EQ(elements_of(response->information_elements).size(), 0u);
}

TEST_F(TestManagement, test_management_association_requests)
{
    const std::vector<uint8_t> assoc = {0x31, 0x04, 0x0a, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> reassoc = {0x31, 0x04, 0x0a, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x00, 0x00};

    const Result<AssocRequest> request = parse_assoc_request(ArrUByteView(assoc));
    const Result<ReassocRequest> reassociation = parse_reassoc_request(ArrUByteView(reassoc));

    ASSERT_TRUE(request.has_value());
    ASSERT_TRUE(reassociation.has_value());
    EXPECT_EQ(request->capability, 0x0431u);
    EXPECT_EQ(request->listen_interval, 10u);
    EXPECT_EQ(elements_of(request->information_elements).size(), 1u);
    EXPECT_EQ(reassociation->listen_interval, 10u);
    EXPECT_EQ(reassociation->current_ap, (MacAddress {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}));
    EXPECT_EQ(elements_of(reassociation->information_elements).size(), 1u);
}

TEST_F(TestManagement, test_management_association_response)
{
    const std::vector<uint8_t> body = {0x31, 0x04, 0x00, 0x00, 0x05, 0xc0, 0x01, 0x00};

    const Result<AssocResponse> response = parse_assoc_response(ArrUByteView(body));

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status, 0u);
    EXPECT_EQ(response->aid, 5u);
    EXPECT_EQ(elements_of(response->information_elements).size(), 1u);
}

TEST_F(TestManagement, test_management_authentication)
{
    const std::vector<uint8_t> body = {0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

    const Result<Authentication> authentication = parse_authentication(ArrUByteView(body));

    ASSERT_TRUE(authentication.has_value());
    EXPECT_EQ(authentication->algorithm, 3u);
    EXPECT_EQ(authentication->sequence, 1u);
    EXPECT_EQ(authentication->status, 0u);
    EXPECT_EQ(elements_of(authentication->information_elements).size(), 1u);
}

TEST_F(TestManagement, test_management_reason)
{
    const std::vector<uint8_t> body = {0x07, 0x00};

    EXPECT_EQ(parse_reason(ArrUByteView(body)), 7u);
}

TEST_F(TestManagement, test_management_action)
{
    const std::vector<uint8_t> body = {0x03, 0x01, 0x02};

    const Result<Action> action = parse_action(ArrUByteView(body));

    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(action->category, 3u);
    EXPECT_EQ(action->body.size(), 2u);
}

TEST_F(TestManagement, test_management_rejects_short_bodies)
{
    const std::vector<uint8_t> one_byte = {0x00};
    const std::vector<uint8_t> nine_bytes(9, 0x00);
    const std::vector<uint8_t> eleven_bytes(11, 0x00);

    EXPECT_EQ(failure_of(parse_beacon(ArrUByteView(eleven_bytes))), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_probe_response(ArrUByteView(eleven_bytes))), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_assoc_request(ArrUByteView(one_byte))), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_reassoc_request(ArrUByteView(nine_bytes))), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_assoc_response(ArrUByteView(one_byte))), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_authentication(ArrUByteView(one_byte))), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_reason(ArrUByteView(one_byte))), ErrorCode::invalid_argument);
    EXPECT_EQ(failure_of(parse_action(ArrUByteView({}))), ErrorCode::invalid_argument);
}

} // namespace test
