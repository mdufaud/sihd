#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/wifi/data.hpp>

#include "wifi_test_helpers.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;

class TestData: public ::testing::Test
{
    protected:
        TestData() { LoggerManager::stream(); }

        virtual ~TestData() { LoggerManager::clear_loggers(); }
};

TEST_F(TestData, test_data_llc_snap_ipv4)
{
    const std::vector<uint8_t> body = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00};

    const Result<Llc> llc = parse_llc_snap(ArrUByteView(body));

    ASSERT_TRUE(llc.has_value());
    EXPECT_EQ(llc->oui, (Oui {0x00, 0x00, 0x00}));
    EXPECT_EQ(llc->ethertype, 0x0800u);
    ASSERT_EQ(llc->payload.size(), 2u);
    EXPECT_EQ(llc->payload[0], 0x45);
}

TEST_F(TestData, test_data_llc_snap_eapol)
{
    const std::vector<uint8_t> body = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e};

    const Result<Llc> llc = parse_llc_snap(ArrUByteView(body));

    ASSERT_TRUE(llc.has_value());
    EXPECT_EQ(llc->ethertype, 0x888eu);
    EXPECT_EQ(llc->payload.size(), 0u);
}

TEST_F(TestData, test_data_llc_rejects_other_headers)
{
    const std::vector<uint8_t> spanning_tree = {0x42, 0x42, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> short_body = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x08};

    EXPECT_EQ(failure_of(parse_llc_snap(ArrUByteView(spanning_tree))), ErrorCode::not_supported);
    EXPECT_EQ(failure_of(parse_llc_snap(ArrUByteView(short_body))), ErrorCode::invalid_argument);
}

} // namespace test
